#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/module.hpp"
#include <dv-sdk/cross/portable_time.h>

#include <cstdio>
#include <ctime>
#include <iostream>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

class LensCalibration : public dv::ModuleBase {
private:
	enum CameraCalibrationPattern { CAMCALIB_CHESSBOARD, CAMCALIB_CIRCLES_GRID, CAMCALIB_ASYMMETRIC_CIRCLES_GRID };

	bool doCalibration;
	char *saveFileName;
	uint32_t captureDelay;
	uint32_t minNumberOfPoints;
	float maxTotalError;
	enum CameraCalibrationPattern calibrationPattern;
	uint32_t boardWidth;
	uint32_t boardHeigth;
	float boardSquareSize;
	float aspectRatio;
	bool assumeZeroTangentialDistortion;
	bool fixPrincipalPointAtCenter;
	bool useFisheyeModel;

	uint32_t imageWidth;
	uint32_t imageHeigth;

	int flag = 0;
	cv::Size boardSize;

	std::vector<std::vector<cv::Point2f>> imagePoints;
	cv::Mat cameraMatrix;
	cv::Mat distCoeffs;

	uint64_t lastFrameTimestamp;
	size_t lastFoundPoints;
	bool calibrationCompleted;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("frames", dv::Frame::identifier, false);
	}

	static const char *getDescription() {
		return ("Lens distortion calibration (use module 'dv_Undistort' to apply undistortion).");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add("hotPixelLearn",
			dv::ConfigOption::boolOption(
				"Learn the position of current hot (abnormally active) pixels, so they can be filtered out.", false,
				dv::ButtonMode::EXECUTE));

		dvConfigNodeCreateBool(
			moduleNode, "doCalibration", false, DVCFG_FLAGS_NORMAL, "Do calibration using live images.");
		dvConfigNodeCreateString(moduleNode, "saveFileName", "camera_calib.xml", 2, PATH_MAX, DVCFG_FLAGS_NORMAL,
			"The name of the file where to write the calculated calibration settings.");
		dvConfigNodeCreateInt(moduleNode, "captureDelay", 500000, 0, 60000000, DVCFG_FLAGS_NORMAL,
			"Only use a frame for calibration if at least this much time has passed.");
		dvConfigNodeCreateInt(moduleNode, "minNumberOfPoints", 20, 3, 100, DVCFG_FLAGS_NORMAL,
			"Minimum number of points to start calibration with.");
		dvConfigNodeCreateFloat(moduleNode, "maxTotalError", 0.30f, 0.0f, 1.0f, DVCFG_FLAGS_NORMAL,
			"Maximum total average error allowed (in pixels).");
		dvConfigNodeCreateString(moduleNode, "calibrationPattern", "chessboard", 10, 21, DVCFG_FLAGS_NORMAL,
			"Pattern to run calibration with.");
		dvConfigNodeAttributeModifierListOptions(
			moduleNode, "calibrationPattern", "chessboard,circlesGrid,asymmetricCirclesGrid", false);
		dvConfigNodeCreateInt(moduleNode, "boardWidth", 9, 1, 64, DVCFG_FLAGS_NORMAL, "The size of the board (width).");
		dvConfigNodeCreateInt(
			moduleNode, "boardHeigth", 5, 1, 64, DVCFG_FLAGS_NORMAL, "The size of the board (heigth).");
		dvConfigNodeCreateFloat(moduleNode, "boardSquareSize", 1.0f, 0.0f, 1000.0f, DVCFG_FLAGS_NORMAL,
			"The size of a square in your defined unit (point, millimeter, etc.).");
		dvConfigNodeCreateFloat(moduleNode, "aspectRatio", 0.0f, 0.0f, 1.0f, DVCFG_FLAGS_NORMAL, "The aspect ratio.");
		dvConfigNodeCreateBool(moduleNode, "assumeZeroTangentialDistortion", false, DVCFG_FLAGS_NORMAL,
			"Assume zero tangential distortion.");
		dvConfigNodeCreateBool(moduleNode, "fixPrincipalPointAtCenter", false, DVCFG_FLAGS_NORMAL,
			"Fix the principal point at the center.");
		dvConfigNodeCreateBool(
			moduleNode, "useFisheyeModel", false, DVCFG_FLAGS_NORMAL, "Use fisheye camera model for calibration.");
	}

	LensCalibration() {
		// Wait for input to be ready. All inputs, once they are up and running, will
		// have a valid sourceInfo node to query, especially if dealing with data.
		// Allocate map using info from sourceInfo.
		auto info = inputs.getInfoNode("events");
		if (!info) {
			throw std::runtime_error("Change events input not ready, upstream module not running.");
		}

		sizeX = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeX"));
		sizeY = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeY"));

		// Populate event output info node, keep same as input info node.
		info.copyTo(outputs.getInfoNode("events"));
	}

	void advancedConfigUpdate() override {
		// Parse calibration pattern string.
		char *calibPattern = dvConfigNodeGetString(moduleData->moduleNode, "calibrationPattern");

		if (caerStrEquals(calibPattern, "chessboard")) {
			state->settings.calibrationPattern = CAMCALIB_CHESSBOARD;
		}
		else if (caerStrEquals(calibPattern, "circlesGrid")) {
			state->settings.calibrationPattern = CAMCALIB_CIRCLES_GRID;
		}
		else if (caerStrEquals(calibPattern, "asymmetricCirclesGrid")) {
			state->settings.calibrationPattern = CAMCALIB_ASYMMETRIC_CIRCLES_GRID;
		}
		else {
			dvModuleLog(moduleData, CAER_LOG_ERROR,
				"Invalid calibration pattern defined. Select one of: 'chessboard', "
				"'circlesGrid' or 'asymmetricCirclesGrid'. Defaulting to "
				"'chessboard'.");

			state->settings.calibrationPattern = CAMCALIB_CHESSBOARD;
		}

		// Reset calibration status after any config change.
		state->lastFrameTimestamp   = 0;
		state->lastFoundPoints      = 0;
		state->calibrationCompleted = false;
		state->calibrationLoaded    = false;

		if (settings->useFisheyeModel) {
			// The fisheye model has its own enum, so overwrite the flags.
			flag = fisheye::CALIB_FIX_SKEW | fisheye::CALIB_RECOMPUTE_EXTRINSIC | fisheye::CALIB_FIX_K2
				   | fisheye::CALIB_FIX_K3 | fisheye::CALIB_FIX_K4;
		}
		else {
			flag = CALIB_FIX_K4 | CALIB_FIX_K5;

			if (settings->aspectRatio == 0.0f) {
				flag |= CALIB_FIX_ASPECT_RATIO;
			}

			if (settings->assumeZeroTangentialDistortion) {
				flag |= CALIB_ZERO_TANGENT_DIST;
			}

			if (settings->fixPrincipalPointAtCenter) {
				flag |= CALIB_FIX_PRINCIPAL_POINT;
			}
		}

		// Update board size.
		boardSize.width  = settings->boardWidth;
		boardSize.height = settings->boardHeigth;

		// Clear current image points.
		imagePoints.clear();
	}

	void run() override {
		auto evt_in  = inputs.get<dv::EventPacket>("events");
		auto evt_out = outputs.get<dv::EventPacket>("events");

		bool hotPixelEnabled = config.get<dvCfgType::BOOL>("hotPixelEnabled");

		// Calibration is done only using frames.
		if (state->settings.doCalibration && !state->calibrationCompleted && frame != NULL) {
			CAER_FRAME_ITERATOR_VALID_START(frame)
			// Only work on new frames if enough time has passed between this and the last used one.
			uint64_t currTimestamp = U64T(caerFrameEventGetTSStartOfFrame64(caerFrameIteratorElement, frame));

			// If enough time has passed, try to add a new point set.
			if ((currTimestamp - state->lastFrameTimestamp) >= state->settings.captureDelay) {
				state->lastFrameTimestamp = currTimestamp;

				bool foundPoint = calibration_findNewPoints(state->cpp_class, caerFrameIteratorElement);
				dvModuleLog(moduleData, CAER_LOG_WARNING, "Searching for new point set, result = %d.", foundPoint);
			}
			CAER_FRAME_ITERATOR_VALID_END

			// If enough points have been found in this round, try doing calibration.
			size_t foundPoints = calibration_foundPoints(state->cpp_class);

			if (foundPoints >= state->settings.minNumberOfPoints && foundPoints > state->lastFoundPoints) {
				state->lastFoundPoints = foundPoints;

				double totalAvgError;
				state->calibrationCompleted = calibration_runCalibrationAndSave(state->cpp_class, &totalAvgError);
				dvModuleLog(moduleData, CAER_LOG_WARNING, "Executing calibration, result = %d, error = %f.",
					state->calibrationCompleted, totalAvgError);
			}
		}
	}

	bool findNewPoints(caerFrameEvent frame) {
		if (frame == NULL || !caerFrameEventIsValid(frame)) {
			return (false);
		}

		// Initialize OpenCV Mat based on caerFrameEvent data directly (no image copy).
		Size frameSize(caerFrameEventGetLengthX(frame), caerFrameEventGetLengthY(frame));
		Mat orig(frameSize, CV_16UC(caerFrameEventGetChannelNumber(frame)), caerFrameEventGetPixelArrayUnsafe(frame));

		// Create a new Mat that has only 8 bit depth from the original 16 bit one.
		// findCorner functions in OpenCV only support 8 bit depth.
		Mat view;
		orig.convertTo(view, CV_8UC(orig.channels()), 1.0 / 256.0);

		int chessBoardFlags = CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE;

		if (!settings->useFisheyeModel) {
			// Fast check erroneously fails with high distortions like fisheye lens.
			chessBoardFlags |= CALIB_CB_FAST_CHECK;
		}

		// Find feature points on the input image.
		vector<Point2f> pointBuf;
		bool found;

		switch (settings->calibrationPattern) {
			case CAMCALIB_CHESSBOARD:
				found = findChessboardCorners(view, boardSize, pointBuf, chessBoardFlags);
				break;

			case CAMCALIB_CIRCLES_GRID:
				found = findCirclesGrid(view, boardSize, pointBuf);
				break;

			case CAMCALIB_ASYMMETRIC_CIRCLES_GRID:
				found = findCirclesGrid(view, boardSize, pointBuf, CALIB_CB_ASYMMETRIC_GRID);
				break;

			default:
				found = false;
				break;
		}

		if (found) {
			// Improve the found corners' coordinate accuracy for chessboard pattern.
			if (settings->calibrationPattern == CAMCALIB_CHESSBOARD) {
				Mat viewGray;

				// Only convert color if not grayscale already.
				if (view.channels() == GRAYSCALE) {
					viewGray = view;
				}
				else {
					if (view.channels() == RGB) {
						cvtColor(view, viewGray, COLOR_RGB2GRAY);
					}
					else if (view.channels() == RGBA) {
						cvtColor(view, viewGray, COLOR_RGBA2GRAY);
					}
				}

				cornerSubPix(viewGray, pointBuf, Size(5, 5), Size(-1, -1),
					TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.1));
			}

			imagePoints.push_back(pointBuf);
		}

		return (found);
	}

	size_t foundPoints(void) {
		return (imagePoints.size());
	}

	double computeReprojectionErrors(const vector<vector<Point3f>> &objectPoints,
		const vector<vector<Point2f>> &imagePoints, const vector<Mat> &rvecs, const vector<Mat> &tvecs,
		const Mat &cameraMatrix, const Mat &distCoeffs, vector<float> &perViewErrors, bool fisheye) {
		vector<Point2f> imagePoints2;
		size_t totalPoints = 0;
		double totalErr    = 0;
		double err;

		perViewErrors.resize(objectPoints.size());

		for (size_t i = 0; i < objectPoints.size(); i++) {
			if (fisheye) {
				fisheye::projectPoints(objectPoints[i], imagePoints2, rvecs[i], tvecs[i], cameraMatrix, distCoeffs);
			}
			else {
				projectPoints(objectPoints[i], rvecs[i], tvecs[i], cameraMatrix, distCoeffs, imagePoints2);
			}

			err = norm(imagePoints[i], imagePoints2, NORM_L2);

			size_t n         = objectPoints[i].size();
			perViewErrors[i] = (float) std::sqrt(err * err / n);
			totalErr += err * err;
			totalPoints += n;
		}

		return (std::sqrt(totalErr / totalPoints));
	}

	void calcBoardCornerPositions(
		Size boardSize, float squareSize, vector<Point3f> &corners, enum CameraCalibrationPattern patternType) {
		corners.clear();

		switch (patternType) {
			case CAMCALIB_CHESSBOARD:
			case CAMCALIB_CIRCLES_GRID:
				for (int y = 0; y < boardSize.height; y++) {
					for (int x = 0; x < boardSize.width; x++) {
						corners.push_back(Point3f(x * squareSize, y * squareSize, 0));
					}
				}
				break;

			case CAMCALIB_ASYMMETRIC_CIRCLES_GRID:
				for (int y = 0; y < boardSize.height; y++) {
					for (int x = 0; x < boardSize.width; x++) {
						corners.push_back(Point3f((2 * x + y % 2) * squareSize, y * squareSize, 0));
					}
				}
				break;

			default:
				break;
		}
	}

	bool runCalibration(Size &imageSize, Mat &cameraMatrix, Mat &distCoeffs, vector<vector<Point2f>> imagePoints,
		vector<float> &reprojErrs, double &totalAvgErr) {
		// 3x3 camera matrix to fill in.
		cameraMatrix = Mat::eye(3, 3, CV_64F);

		if (flag & CALIB_FIX_ASPECT_RATIO) {
			cameraMatrix.at<double>(0, 0) = settings->aspectRatio;
		}

		if (settings->useFisheyeModel) {
			distCoeffs = Mat::zeros(4, 1, CV_64F);
		}
		else {
			distCoeffs = Mat::zeros(8, 1, CV_64F);
		}

		vector<vector<Point3f>> objectPoints(1);

		calcBoardCornerPositions(boardSize, settings->boardSquareSize, objectPoints[0], settings->calibrationPattern);

		objectPoints.resize(imagePoints.size(), objectPoints[0]);

		// Find intrinsic and extrinsic camera parameters.
		vector<Mat> rvecs, tvecs;

		if (settings->useFisheyeModel) {
			Mat _rvecs, _tvecs;
			fisheye::calibrate(objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, _rvecs, _tvecs, flag);

			rvecs.reserve(_rvecs.rows);
			tvecs.reserve(_tvecs.rows);

			for (size_t i = 0; i < objectPoints.size(); i++) {
				rvecs.push_back(_rvecs.row(i));
				tvecs.push_back(_tvecs.row(i));
			}
		}
		else {
			calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flag);
		}

		totalAvgErr = computeReprojectionErrors(
			objectPoints, imagePoints, rvecs, tvecs, cameraMatrix, distCoeffs, reprojErrs, settings->useFisheyeModel);

		bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs) && totalAvgErr < settings->maxTotalError;

		return (ok);
	}

	// Print camera parameters to the output file
	bool saveCameraParams(
		Size &imageSize, Mat &cameraMatrix, Mat &distCoeffs, const vector<float> &reprojErrs, double totalAvgErr) {
		FileStorage fs(settings->saveFileName, FileStorage::WRITE);

		// Check file.
		if (!fs.isOpened()) {
			return (false);
		}

		struct tm currentTimeStruct = portable_clock_localtime();

		char buf[1024];
		strftime(buf, sizeof(buf) - 1, "%c", &currentTimeStruct);

		fs << "calibration_time" << buf;

		if (!reprojErrs.empty()) {
			fs << "nr_of_frames" << (int) reprojErrs.size();
		}

		fs << "image_width" << imageSize.width;
		fs << "image_height" << imageSize.height;
		fs << "board_width" << boardSize.width;
		fs << "board_height" << boardSize.height;
		fs << "square_size" << settings->boardSquareSize;

		if (flag & CALIB_FIX_ASPECT_RATIO) {
			fs << "aspect_ratio" << settings->aspectRatio;
		}

		if (flag) {
			if (settings->useFisheyeModel) {
				sprintf(buf, "flags:%s%s%s%s%s%s", flag & fisheye::CALIB_FIX_SKEW ? " +fix_skew" : "",
					flag & fisheye::CALIB_FIX_K1 ? " +fix_k1" : "", flag & fisheye::CALIB_FIX_K2 ? " +fix_k2" : "",
					flag & fisheye::CALIB_FIX_K3 ? " +fix_k3" : "", flag & fisheye::CALIB_FIX_K4 ? " +fix_k4" : "",
					flag & fisheye::CALIB_RECOMPUTE_EXTRINSIC ? " +recompute_extrinsic" : "");
			}
			else {
				sprintf(buf, "flags:%s%s%s%s%s%s%s%s%s%s",
					flag & CALIB_USE_INTRINSIC_GUESS ? " +use_intrinsic_guess" : "",
					flag & CALIB_FIX_ASPECT_RATIO ? " +fix_aspect_ratio" : "",
					flag & CALIB_FIX_PRINCIPAL_POINT ? " +fix_principal_point" : "",
					flag & CALIB_ZERO_TANGENT_DIST ? " +zero_tangent_dist" : "", flag & CALIB_FIX_K1 ? " +fix_k1" : "",
					flag & CALIB_FIX_K2 ? " +fix_k2" : "", flag & CALIB_FIX_K3 ? " +fix_k3" : "",
					flag & CALIB_FIX_K4 ? " +fix_k4" : "", flag & CALIB_FIX_K5 ? " +fix_k5" : "",
					flag & CALIB_FIX_K6 ? " +fix_k6" : "");
			}

#if (CV_VERSION_MAJOR >= 4) || (CV_VERSION_MAJOR == 3 && CV_VERSION_MINOR >= 2)
			fs.writeComment(buf, 0);
#else
			cvWriteComment(*fs, buf, 0);
#endif
		}

		fs << "flags" << flag;

		fs << "use_fisheye_model" << settings->useFisheyeModel;

		fs << "camera_matrix" << cameraMatrix;
		fs << "distortion_coefficients" << distCoeffs;

		fs << "avg_reprojection_error" << totalAvgErr;

		if (!reprojErrs.empty()) {
			fs << "per_view_reprojection_errors" << Mat(reprojErrs);
		}

		// Close file.
		fs.release();

		return (true);
	}

	bool runCalibrationAndSave(double *totalAvgError) {
		// Only run if enough valid points have been accumulated.
		if (imagePoints.size() < settings->minNumberOfPoints) {
			return (false);
		}

		// Check that image size is properly defined.
		if (settings->imageWidth <= 0 || settings->imageHeigth <= 0) {
			return (false);
		}

		Size imageSize(settings->imageWidth, settings->imageHeigth);
		vector<float> reprojErrs;
		*totalAvgError = 0;

		bool ok = runCalibration(imageSize, cameraMatrix, distCoeffs, imagePoints, reprojErrs, *totalAvgError);

		if (ok) {
			ok = saveCameraParams(imageSize, cameraMatrix, distCoeffs, reprojErrs, *totalAvgError);
		}

		return (ok);
	}
};

registerModuleClass(LensCalibration)
