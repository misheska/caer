#include "dv-sdk/data/event.hpp"
#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/module.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>

class Undistort : public dv::ModuleBase {
private:
	int16_t eventSizeX;
	int16_t eventSizeY;
	int16_t frameSizeX;
	int16_t frameSizeY;

	bool calibrationLoaded;

	std::vector<cv::Point2i> undistortEventMap;
	cv::Mat undistortFrameRemap1;
	cv::Mat undistortFrameRemap2;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("events", dv::EventPacket::identifier, true);
		in.emplace_back("frames", dv::Frame::identifier, true);
	}

	static void addOutputs(std::vector<dv::OutputDefinition> &out) {
		out.emplace_back("undistortedEvents", dv::EventPacket::identifier);
		out.emplace_back("undistortedFrames", dv::Frame::identifier);
	}

	static const char *getDescription() {
		return (
			"Remove distortion from lens in both frames and events (use module 'dv_LensCalibration' for calibration).");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add(
			"fitAllPixels", dv::ConfigOption::boolOption("Whether to fit all the input pixels (black borders) or "
														 "maximize the image, at the cost of loosing some pixels.",
								false));
		config.add("calibrationFile",
			dv::ConfigOption::fileOpenOption(
				"The name of the file from which to load the calibration settings for undistortion.", "xml"));
	}

	Undistort() : calibrationLoaded(false) {
		// Wait for input to be ready. All inputs, once they are up and running, will
		// have a valid sourceInfo node to query, especially if dealing with data.
		bool eventsConnected = inputs.isConnected("events");
		bool framesConnected = inputs.isConnected("frames");

		if (!eventsConnected && !framesConnected) {
			throw std::runtime_error("No input is connected, nothing to do.");
		}

		if (eventsConnected) {
			auto eventsInfo = inputs.getInfo("events");
			if (!eventsInfo) {
				throw std::runtime_error("Events input not ready, upstream module not running.");
			}

			eventSizeX = static_cast<int16_t>(eventsInfo.get<dv::CfgType::INT>("sizeX"));
			eventSizeY = static_cast<int16_t>(eventsInfo.get<dv::CfgType::INT>("sizeY"));

			// Populate event output info node, keep same as input info node.
			eventsInfo.copyTo(outputs.getInfo("undistortedEvents"));
		}

		if (framesConnected) {
			auto framesInfo = inputs.getInfo("frames");
			if (!framesInfo) {
				throw std::runtime_error("Frames input not ready, upstream module not running.");
			}

			frameSizeX = static_cast<int16_t>(framesInfo.get<dv::CfgType::INT>("sizeX"));
			frameSizeY = static_cast<int16_t>(framesInfo.get<dv::CfgType::INT>("sizeY"));

			// Populate event output info node, keep same as input info node.
			framesInfo.copyTo(outputs.getInfo("undistortedFrames"));
		}
	}

	void configUpdate() override {
		// Any changes to configuration mean the calibration has to be
		// reloaded and reinitialized, so we force this here.
		calibrationLoaded = false;
	}

	void run() override {
		auto events_in  = inputs.get<dv::EventPacket>("events");
		auto events_out = outputs.get<dv::EventPacket>("undistortedEvents");

		auto frame_in  = inputs.get<dv::Frame>("frames");
		auto frame_out = outputs.get<dv::Frame>("undistortedFrames");

		// At this point we always try to load the calibration settings for undistortion.
		// Maybe they just got created or exist from a previous run.
		if (!calibrationLoaded) {
			calibrationLoaded = loadUndistortMatrices();
		}

		// Undistortion can be applied to both frames and events.
		if (calibrationLoaded) {
			if (events_in) {
				undistortEvents(events_in, events_out);
			}

			if (frame_in) {
				undistortFrame(frame_in, frame_out);
			}
		}
	}

	bool loadUndistortMatrices() {
		// Open file with undistort matrices.
		cv::FileStorage fs(config.get<dv::CfgType::STRING>("calibrationFile"), cv::FileStorage::READ);

		// Check file.
		if (!fs.isOpened()) {
			return (false);
		}

		cv::Mat undistortCameraMatrix;
		cv::Mat undistortDistCoeffs;
		bool useFisheyeModel = false;

		fs["camera_matrix"] >> undistortCameraMatrix;
		fs["distortion_coefficients"] >> undistortDistCoeffs;
		fs["use_fisheye_model"] >> useFisheyeModel;

		// Close file.
		fs.release();

		// Generate maps for frame remap().
		cv::Size frameSize(frameSizeX, frameSizeY);
		cv::Size eventSize(eventSizeX, eventSizeY);

		// Allocate undistort events maps.
		std::vector<cv::Point2f> undistortEventInputMap;
		undistortEventInputMap.reserve(static_cast<size_t>(eventSize.area()));

		std::vector<cv::Point2f> undistortEventOutputMap;
		undistortEventOutputMap.reserve(static_cast<size_t>(eventSize.area()));

		// Populate undistort events input map with all possible (x, y) address combinations.
		for (int y = 0; y < eventSize.height; y++) {
			for (int x = 0; x < eventSize.width; x++) {
				// Use center of pixel to get better approximation, since we're using floats anyway.
				undistortEventInputMap.push_back(
					cv::Point2f(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f));
			}
		}

		if (useFisheyeModel) {
			bool fitAllPixels = config.get<dv::CfgType::BOOL>("fitAllPixels");

			cv::Mat optimalCameramatrixEvents;
			cv::Mat optimalCameramatrixFrames;

			if (fitAllPixels) {
				cv::fisheye::estimateNewCameraMatrixForUndistortRectify(undistortCameraMatrix, undistortDistCoeffs,
					eventSize, cv::Matx33d::eye(), optimalCameramatrixEvents, 1);

				cv::fisheye::estimateNewCameraMatrixForUndistortRectify(undistortCameraMatrix, undistortDistCoeffs,
					frameSize, cv::Matx33d::eye(), optimalCameramatrixFrames, 1);
			}

			cv::fisheye::initUndistortRectifyMap(undistortCameraMatrix, undistortDistCoeffs, cv::Matx33d::eye(),
				(fitAllPixels) ? (optimalCameramatrixFrames) : (undistortCameraMatrix), frameSize, CV_16SC2,
				undistortFrameRemap1, undistortFrameRemap2);

			cv::fisheye::undistortPoints(undistortEventInputMap, undistortEventOutputMap, undistortCameraMatrix,
				undistortDistCoeffs, cv::Matx33d::eye(),
				(fitAllPixels) ? (optimalCameramatrixEvents) : (undistortCameraMatrix));
		}
		else {
			// fitAllPixels is not supported for standard lenses. The computation looks strange for APS frames
			// and completely fails for DVS events.
			cv::initUndistortRectifyMap(undistortCameraMatrix, undistortDistCoeffs, cv::Matx33d::eye(),
				undistortCameraMatrix, frameSize, CV_16SC2, undistortFrameRemap1, undistortFrameRemap2);

			cv::undistortPoints(undistortEventInputMap, undistortEventOutputMap, undistortCameraMatrix,
				undistortDistCoeffs, cv::Matx33d::eye(), undistortCameraMatrix);
		}

		// Convert undistortEventOutputMap to integer from float for faster calculations later on.
		undistortEventMap.clear();
		undistortEventMap.reserve(static_cast<size_t>(eventSize.area()));

		for (size_t i = 0; i < undistortEventOutputMap.size(); i++) {
			undistortEventMap.push_back(undistortEventOutputMap.at(i));
		}

		return (true);
	}

	void undistortEvents(dv::InputWrapper<dv::EventPacket> &in, dv::OutputWrapper<dv::EventPacket> &out) {
		for (const auto &evt : in) {
			// Get new coordinates at which event shall be remapped.
			size_t mapIdx              = static_cast<size_t>((evt.y() * eventSizeX) + evt.x());
			cv::Point2i eventUndistort = undistortEventMap.at(mapIdx);

			// Check that new coordinates are still within view boundary. If yes, use new remapped coordinates.
			if (eventUndistort.x >= 0 && eventUndistort.x < eventSizeX && eventUndistort.y >= 0
				&& eventUndistort.y < eventSizeY) {
				out.emplace_back(evt.timestamp(), static_cast<int16_t>(eventUndistort.x),
					static_cast<int16_t>(eventUndistort.y), evt.polarity());
			}
		}

		out.commit();
	}

	void undistortFrame(dv::InputWrapper<dv::Frame> &in, dv::OutputWrapper<dv::Frame> &out) {
		// Setup output frame. Same size.
		out->sizeX                    = in->sizeX;
		out->sizeY                    = in->sizeY;
		out->format                   = in->format;
		out->positionX                = in->positionX;
		out->positionY                = in->positionY;
		out->timestamp                = in->timestamp;
		out->timestampStartOfFrame    = in->timestampStartOfFrame;
		out->timestampEndOfFrame      = in->timestampEndOfFrame;
		out->timestampStartOfExposure = in->timestampStartOfExposure;
		out->timestampEndOfExposure   = in->timestampEndOfExposure;
		out->pixels.resize(in->pixels.size()); // Allocate memory (same number of channels, same size).

		// Get input OpenCV Mat. Lifetime is properly managed.
		auto input = in.getMatPointer();

		// Get output OpenCV Mat. Memory must have been allocated already.
		auto output = out.getMat();

		cv::remap(*input, output, undistortFrameRemap1, undistortFrameRemap2, cv::INTER_CUBIC, cv::BORDER_CONSTANT);

		out.commit();
	}
};

registerModuleClass(Undistort)
