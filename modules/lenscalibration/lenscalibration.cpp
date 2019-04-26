#include "dv-sdk/cross/portable_time.h"
#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/module.hpp"

#include <cstdio>
#include <ctime>
#include <iostream>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>

class LensCalibration : public dv::ModuleBase {
private:
	cv::Size imageSize;
	cv::Size boardSize;
	int flag;

	enum class CalibrationPatterns { CHESSBOARD, CIRCLES_GRID, ASYMMETRIC_CIRCLES_GRID } calibrationPattern;

	std::vector<std::vector<cv::Point2f>> imagePoints;
	cv::Mat cameraMatrix;
	cv::Mat distCoeffs;

	int64_t lastFrameTimestamp;
	size_t lastFoundPoints;
	bool calibrationCompleted;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("frames", dv::Frame::identifier, false);
	}

	static void addOutputs(std::vector<dv::OutputDefinition> &out) {
		out.emplace_back("patternCorners", dv::Frame::identifier);
	}

	static const char *getDescription() {
		return ("Lens distortion calibration (use module 'dv_Undistort' to apply undistortion).");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add("calibrationFile",
			dv::ConfigOption::fileOpenOption(
				"The name of the file from which to load the calibration settings for undistortion.", "xml"));
		config.add(
			"calibrationPattern", dv::ConfigOption::listOption("Pattern to run calibration with.", 0,
									  std::vector<std::string>{"chessboard", "circlesGrid", "asymmetricCirclesGrid"}));
		config.add("boardWidth", dv::ConfigOption::intOption("The cv::Size of the board (width).", 9, 1, 64));
		config.add("boardHeigth", dv::ConfigOption::intOption("The cv::Size of the board (height).", 5, 1, 64));
		config.add("boardSquareSize",
			dv::ConfigOption::floatOption(
				"The cv::Size of a square in your defined unit (point, millimeter, etc.).", 1.0f, 0.0f, 1000.0f));
		config.add("aspectRatio", dv::ConfigOption::floatOption("The aspect ratio.", 0.0f, 0.0f, 1.0f));
		config.add("maxTotalError",
			dv::ConfigOption::floatOption("Maximum total average error allowed (in pixels).", 0.30f, 0.0f, 1.0f));
		config.add("assumeZeroTangentialDistortion",
			dv::ConfigOption::boolOption("Assume zero tangential distortion.", false));
		config.add(
			"fixPrincipalPointAtCenter", dv::ConfigOption::boolOption("Fix the principal point at the center.", false));
		config.add("useFisheyeModel", dv::ConfigOption::boolOption("Use fisheye camera model for calibration.", false));
		config.add("captureInterval",
			dv::ConfigOption::intOption(
				"Only use a frame for calibration if at least this much time has passed, in µs.", 500000, 0, 60000000));
		config.add("minNumberOfPoints",
			dv::ConfigOption::intOption("Minimum number of points to start calibration with.", 20, 3, 100));
	}

	LensCalibration() {
		imageSize = inputs.getFrameInput("frames").size();
		inputs.getFrameInput("frames").infoNode().copyTo(outputs.getInfo("patternCorners"));

		configUpdate();
	}

	void configUpdate() override {
		// Parse available choices into enum value.
		auto selectedCalibrationPattern = config.get<dv::CfgType::STRING>("calibrationPattern");

		if (selectedCalibrationPattern == "circlesGrid") {
			calibrationPattern = CalibrationPatterns::CIRCLES_GRID;
		}
		else if (selectedCalibrationPattern == "asymmetricCirclesGrid") {
			calibrationPattern = CalibrationPatterns::ASYMMETRIC_CIRCLES_GRID;
		}
		else {
			// Default choice.
			calibrationPattern = CalibrationPatterns::CHESSBOARD;
		}

		if (config.get<dv::CfgType::BOOL>("useFisheyeModel")) {
			// The fisheye model has its own enum, so overwrite the flags.
			flag = cv::fisheye::CALIB_FIX_SKEW | cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC | cv::fisheye::CALIB_FIX_K2
				   | cv::fisheye::CALIB_FIX_K3 | cv::fisheye::CALIB_FIX_K4;
		}
		else {
			flag = cv::CALIB_FIX_K4 | cv::CALIB_FIX_K5;

			if (config.get<dv::CfgType::FLOAT>("aspectRatio") != 0.0f) {
				flag |= cv::CALIB_FIX_ASPECT_RATIO;
			}

			if (config.get<dv::CfgType::BOOL>("assumeZeroTangentialDistortion")) {
				flag |= cv::CALIB_ZERO_TANGENT_DIST;
			}
		}

		if (config.get<dv::CfgType::BOOL>("fixPrincipalPointAtCenter")) {
			flag |= cv::CALIB_FIX_PRINCIPAL_POINT;
		}

		boardSize = cv::Size(config.get<dv::CfgType::INT>("boardWidth"), config.get<dv::CfgType::INT>("boardHeight"));

		// Reset calibration status after any config change.
		lastFrameTimestamp   = 0;
		lastFoundPoints      = 0;
		calibrationCompleted = false;

		// Clear current image points.
		imagePoints.clear();
	}

	void run() override {
		auto frame_in = inputs.getFrameInput("frames").data();

		auto corners_out = outputs.get<dv::Frame>("patternCorners");

		// Calibration is done only using frames.
		if (!calibrationCompleted) {
			// Only work on new frames if enough time has passed between this and the last used one.
			int64_t currTimestamp = frame_in->timestamp;

			// If enough time has passed, try to add a new point set.
			if ((currTimestamp - lastFrameTimestamp) >= config.get<dv::CfgType::INT>("captureInterval")) {
				lastFrameTimestamp = currTimestamp;

				bool foundPoint = findNewPoints(frame_in, corners_out);
				log.warning << "Searching for new point set, result = " << foundPoint << "." << dv::logEnd;
			}

			// If enough points have been found in this round, try doing calibration.
			size_t foundPoints = imagePoints.size();

			if (foundPoints >= static_cast<size_t>(config.get<dv::CfgType::INT>("minNumberOfPoints"))
				&& foundPoints > lastFoundPoints) {
				lastFoundPoints = foundPoints;

				double totalAvgError;
				calibrationCompleted = runCalibrationAndSave(&totalAvgError);
				log.warning << "Executing calibration, result = " << calibrationCompleted
							<< ", error = " << totalAvgError << "." << dv::logEnd;
			}
		}
	}

	bool findNewPoints(dv::InputDataWrapper<dv::Frame> &frame, dv::OutputDataWrapper<dv::Frame> &corners) {
		auto view = frame.getMatPointer();

		int chessBoardFlags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE;

		if (!config.get<dv::CfgType::BOOL>("useFisheyeModel")) {
			// Fast check erroneously fails with high distortions like fisheye lens.
			chessBoardFlags |= cv::CALIB_CB_FAST_CHECK;
		}

		// Find feature points on the input image.
		std::vector<cv::Point2f> pointBuf;
		bool found;

		switch (calibrationPattern) {
			case CalibrationPatterns::CHESSBOARD:
				found = cv::findChessboardCorners(*view, boardSize, pointBuf, chessBoardFlags);
				break;

			case CalibrationPatterns::CIRCLES_GRID:
				found = cv::findCirclesGrid(*view, boardSize, pointBuf);
				break;

			case CalibrationPatterns::ASYMMETRIC_CIRCLES_GRID:
				found = cv::findCirclesGrid(*view, boardSize, pointBuf, cv::CALIB_CB_ASYMMETRIC_GRID);
				break;

			default:
				found = false;
				break;
		}

		if (found) {
			// Improve the found corners' coordinate accuracy for chessboard pattern.
			if (calibrationPattern == CalibrationPatterns::CHESSBOARD) {
				cv::Mat viewGray;

				// Only convert color if not grayscale already.
				if (view->channels() == 1) {
					viewGray = *view;
				}
				else {
					if (view->channels() == 3) {
						cv::cvtColor(*view, viewGray, cv::COLOR_BGR2GRAY);
					}
					else if (view->channels() == 4) {
						cv::cvtColor(*view, viewGray, cv::COLOR_BGRA2GRAY);
					}
				}

				cv::cornerSubPix(viewGray, pointBuf, cv::Size(11, 11), cv::Size(-1, -1),
					cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1));
			}

			imagePoints.push_back(pointBuf);

			// Setup output frame. Same size.
			corners->sizeX                    = frame->sizeX;
			corners->sizeY                    = frame->sizeY;
			corners->format                   = frame->format;
			corners->positionX                = frame->positionX;
			corners->positionY                = frame->positionY;
			corners->timestamp                = frame->timestamp;
			corners->timestampStartOfFrame    = frame->timestampStartOfFrame;
			corners->timestampEndOfFrame      = frame->timestampEndOfFrame;
			corners->timestampStartOfExposure = frame->timestampStartOfExposure;
			corners->timestampEndOfExposure   = frame->timestampEndOfExposure;

			// Copy image.
			corners->pixels = frame->pixels;

			auto corners_paint = corners.getMat();

			// Draw the corners.
			cv::drawChessboardCorners(corners_paint, boardSize, cv::Mat(pointBuf), found);

			corners.commit();
		}

		return (found);
	}

	static double computeReprojectionErrors(const std::vector<std::vector<cv::Point3f>> &objectPoints,
		const std::vector<std::vector<cv::Point2f>> &imagePoints, const std::vector<cv::Mat> &rvecs,
		const std::vector<cv::Mat> &tvecs, const cv::Mat &cameraMatrix, const cv::Mat &distCoeffs,
		std::vector<float> &perViewErrors, bool fisheye) {
		std::vector<cv::Point2f> imagePoints2;
		size_t totalPoints = 0;
		double totalErr    = 0;

		perViewErrors.resize(objectPoints.size());

		for (size_t i = 0; i < objectPoints.size(); i++) {
			if (fisheye) {
				cv::fisheye::projectPoints(objectPoints[i], imagePoints2, rvecs[i], tvecs[i], cameraMatrix, distCoeffs);
			}
			else {
				cv::projectPoints(objectPoints[i], rvecs[i], tvecs[i], cameraMatrix, distCoeffs, imagePoints2);
			}

			double err = cv::norm(imagePoints[i], imagePoints2, cv::NORM_L2);

			size_t n         = objectPoints[i].size();
			perViewErrors[i] = static_cast<float>(std::sqrt(err * err / static_cast<double>(n)));
			totalErr += err * err;
			totalPoints += n;
		}

		return (std::sqrt(totalErr / static_cast<double>(totalPoints)));
	}

	void calcBoardCornerPositions(std::vector<cv::Point3f> &corners) {
		corners.clear();

		float squareSize = config.get<dv::CfgType::FLOAT>("boardSquareSize");

		switch (calibrationPattern) {
			case CalibrationPatterns::CHESSBOARD:
			case CalibrationPatterns::CIRCLES_GRID:
				for (int y = 0; y < boardSize.height; y++) {
					for (int x = 0; x < boardSize.width; x++) {
						corners.push_back(
							cv::Point3f(static_cast<float>(x) * squareSize, static_cast<float>(y) * squareSize, 0));
					}
				}
				break;

			case CalibrationPatterns::ASYMMETRIC_CIRCLES_GRID:
				for (int y = 0; y < boardSize.height; y++) {
					for (int x = 0; x < boardSize.width; x++) {
						corners.push_back(cv::Point3f(
							static_cast<float>(2 * x + y % 2) * squareSize, static_cast<float>(y) * squareSize, 0));
					}
				}
				break;

			default:
				break;
		}
	}

	bool runCalibration(std::vector<float> &reprojErrs, double &totalAvgErr) {
		// 3x3 camera matrix to fill in.
		cameraMatrix = cv::Mat::eye(3, 3, CV_64F);

		if (flag & cv::CALIB_FIX_ASPECT_RATIO) {
			cameraMatrix.at<double>(0, 0) = static_cast<double>(config.get<dv::CfgType::FLOAT>("aspectRatio"));
		}

		if (config.get<dv::CfgType::BOOL>("useFisheyeModel")) {
			distCoeffs = cv::Mat::zeros(4, 1, CV_64F);
		}
		else {
			distCoeffs = cv::Mat::zeros(8, 1, CV_64F);
		}

		std::vector<std::vector<cv::Point3f>> objectPoints(1);

		calcBoardCornerPositions(objectPoints[0]);

		objectPoints.resize(imagePoints.size(), objectPoints[0]);

		// Find intrinsic and extrinsic camera parameters.
		std::vector<cv::Mat> rvecs, tvecs;

		if (config.get<dv::CfgType::BOOL>("useFisheyeModel")) {
			cv::Mat _rvecs, _tvecs;
			cv::fisheye::calibrate(
				objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, _rvecs, _tvecs, flag);

			rvecs.reserve(static_cast<size_t>(_rvecs.rows));
			tvecs.reserve(static_cast<size_t>(_tvecs.rows));

			for (size_t i = 0; i < objectPoints.size(); i++) {
				rvecs.push_back(_rvecs.row(static_cast<int>(i)));
				tvecs.push_back(_tvecs.row(static_cast<int>(i)));
			}
		}
		else {
			cv::calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flag);
		}

		totalAvgErr = computeReprojectionErrors(objectPoints, imagePoints, rvecs, tvecs, cameraMatrix, distCoeffs,
			reprojErrs, config.get<dv::CfgType::BOOL>("useFisheyeModel"));

		bool ok = cv::checkRange(cameraMatrix) && cv::checkRange(distCoeffs)
				  && (totalAvgErr < static_cast<double>(config.get<dv::CfgType::FLOAT>("maxTotalError")));

		return (ok);
	}

	// Print camera parameters to the output file
	bool saveCameraParams(const std::vector<float> &reprojErrs, double totalAvgErr) {
		cv::FileStorage fs(config.get<dv::CfgType::STRING>("calibrationFile"), cv::FileStorage::WRITE);

		// Check file.
		if (!fs.isOpened()) {
			return (false);
		}

		struct tm currentTimeStruct = portable_clock_localtime();

		char buf[1024];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-y2k"
		strftime(buf, sizeof(buf) - 1, "%c", &currentTimeStruct);
#pragma GCC diagnostic pop

		fs << "calibration_time" << buf;

		if (!reprojErrs.empty()) {
			fs << "nr_of_frames" << static_cast<int>(reprojErrs.size());
		}

		fs << "image_width" << imageSize.width;
		fs << "image_height" << imageSize.height;
		fs << "board_width" << boardSize.width;
		fs << "board_height" << boardSize.height;
		fs << "square_size" << config.get<dv::CfgType::FLOAT>("boardSquareSize");

		if (flag & cv::CALIB_FIX_ASPECT_RATIO) {
			fs << "aspect_ratio" << config.get<dv::CfgType::FLOAT>("aspectRatio");
		}

		if (flag) {
			if (config.get<dv::CfgType::BOOL>("useFisheyeModel")) {
				sprintf(buf, "flags:%s%s%s%s%s%s", (flag & cv::fisheye::CALIB_FIX_SKEW) ? " +fix_skew" : "",
					(flag & cv::fisheye::CALIB_FIX_K1) ? " +fix_k1" : "",
					(flag & cv::fisheye::CALIB_FIX_K2) ? " +fix_k2" : "",
					(flag & cv::fisheye::CALIB_FIX_K3) ? " +fix_k3" : "",
					(flag & cv::fisheye::CALIB_FIX_K4) ? " +fix_k4" : "",
					(flag & cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC) ? " +recompute_extrinsic" : "");
			}
			else {
				sprintf(buf, "flags:%s%s%s%s%s%s%s%s%s%s",
					(flag & cv::CALIB_USE_INTRINSIC_GUESS) ? " +use_intrinsic_guess" : "",
					(flag & cv::CALIB_FIX_ASPECT_RATIO) ? " +fix_aspect_ratio" : "",
					(flag & cv::CALIB_FIX_PRINCIPAL_POINT) ? " +fix_principal_point" : "",
					(flag & cv::CALIB_ZERO_TANGENT_DIST) ? " +zero_tangent_dist" : "",
					(flag & cv::CALIB_FIX_K1) ? " +fix_k1" : "", (flag & cv::CALIB_FIX_K2) ? " +fix_k2" : "",
					(flag & cv::CALIB_FIX_K3) ? " +fix_k3" : "", (flag & cv::CALIB_FIX_K4) ? " +fix_k4" : "",
					(flag & cv::CALIB_FIX_K5) ? " +fix_k5" : "", (flag & cv::CALIB_FIX_K6) ? " +fix_k6" : "");
			}

#if (CV_VERSION_MAJOR == 3 && CV_VERSION_MINOR <= 1)
			cvWriteComment(*fs, buf, 0);
#else
			fs.writeComment(buf, 0);
#endif
		}

		fs << "flags" << flag;

		fs << "use_fisheye_model" << config.get<dv::CfgType::BOOL>("useFisheyeModel");

		fs << "camera_matrix" << cameraMatrix;
		fs << "distortion_coefficients" << distCoeffs;

		fs << "avg_reprojection_error" << totalAvgErr;

		if (!reprojErrs.empty()) {
			fs << "per_view_reprojection_errors" << cv::Mat(reprojErrs);
		}

		// Close file.
		fs.release();

		return (true);
	}

	bool runCalibrationAndSave(double *totalAvgError) {
		std::vector<float> reprojErrs;
		*totalAvgError = 0;

		bool ok = runCalibration(reprojErrs, *totalAvgError);

		if (ok) {
			ok = saveCameraParams(reprojErrs, *totalAvgError);
		}

		return (ok);
	}
};

registerModuleClass(LensCalibration)
