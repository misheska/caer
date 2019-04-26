#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/module.hpp"

#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

class FrameEnhancer : public dv::ModuleBase {
private:
	enum class ContrastAlgorithms { NORMALIZATION, HISTOGRAM_EQUALIZATION, CLAHE } contrastAlgo;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("frames", dv::Frame::identifier, false);
	}

	static void addOutputs(std::vector<dv::OutputDefinition> &out) {
		out.emplace_back("frames", dv::Frame::identifier);
	}

	static const char *getDescription() {
		return ("Enhance images by applying contrast enhancement algorithms.");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add(
			"contrastAlgorithm", dv::ConfigOption::listOption("Contrast enhancement algorithm to apply.", 0,
									 std::vector<std::string>{"normalization", "histogram_equalization", "clahe"}));
	}

	FrameEnhancer() {
		// setup output frame stream
		outputs.getFrameOutput("frames").setup(inputs.getFrameInput("frames"));

		// Call once to translate string into enum properly.
		configUpdate();
	}

	void run() override {
		auto frame_in  = inputs.getFrameInput("frames").data();
		auto frame_out = outputs.getFrameOutput("frames").getOutputData();

		// Setup output frame. Same size.
		frame_out->sizeX                    = frame_in->sizeX;
		frame_out->sizeY                    = frame_in->sizeY;
		frame_out->format                   = frame_in->format;
		frame_out->positionX                = frame_in->positionX;
		frame_out->positionY                = frame_in->positionY;
		frame_out->timestamp                = frame_in->timestamp;
		frame_out->timestampStartOfFrame    = frame_in->timestampStartOfFrame;
		frame_out->timestampEndOfFrame      = frame_in->timestampEndOfFrame;
		frame_out->timestampStartOfExposure = frame_in->timestampStartOfExposure;
		frame_out->timestampEndOfExposure   = frame_in->timestampEndOfExposure;
		frame_out->pixels.resize(frame_in->pixels.size()); // Allocate memory (same number of channels, same size).

		// Get input OpenCV Mat. Lifetime is properly managed.
		auto input = frame_in.getMatPointer();

		// Get output OpenCV Mat. Memory must have been allocated already.
		auto output = frame_out.getMat();

		CV_Assert((input->type() == CV_8UC1) || (input->type() == CV_8UC3) || (input->type() == CV_8UC4));
		CV_Assert((output.type() == CV_8UC1) || (output.type() == CV_8UC3) || (output.type() == CV_8UC4));

		CV_Assert(input->type() == output.type());
		CV_Assert(input->channels() == output.channels());

		// This generally only works well on grayscale intensity images.
		// So, if this is a grayscale image, good, else if its a color
		// image we convert it to YCrCb and operate on the Y channel only.
		const cv::Mat *intensity;
		cv::Mat yCrCbPlanes[3];
		cv::Mat rgbaAlpha;

		// Grayscale, no intensity extraction needed.
		if (input->channels() == 1) {
			intensity = input.get();
		}
		else {
			// Color image, extract RGB and intensity/luminance.
			cv::Mat rgb;

			if (input->channels() == 4) {
				// We separate Alpha from RGB first.
				// We will restore alpha at the end.
				rgb       = cv::Mat(input->rows, input->cols, CV_8UC3);
				rgbaAlpha = cv::Mat(input->rows, input->cols, CV_8UC1);

				cv::Mat out[] = {rgb, rgbaAlpha};
				// rgba[0] -> rgb[0], rgba[1] -> rgb[1],
				// rgba[2] -> rgb[2], rgba[3] -> rgbaAlpha[0]
				int channelTransform[] = {0, 0, 1, 1, 2, 2, 3, 3};
				mixChannels(input.get(), 1, out, 2, channelTransform, 4);
			}
			else {
				// Already an RGB image.
				rgb = *input;
				CV_Assert(rgb.type() == CV_8UC3);
			}

			// First we convert from RGB to a color space with
			// separate luminance channel.
			cv::Mat rgbYCrCb;
			cvtColor(rgb, rgbYCrCb, cv::COLOR_RGB2YCrCb);

			CV_Assert(rgbYCrCb.type() == CV_8UC3);

			// Then we split it up so that we can access the luminance
			// channel on its own separately.
			split(rgbYCrCb, yCrCbPlanes);

			// Now we have the luminance image in yCrCbPlanes[0].
			intensity = &yCrCbPlanes[0];
		}

		CV_Assert(intensity->type() == CV_8UC1);

		// Apply contrast enhancement algorithm.
		switch (contrastAlgo) {
			case ContrastAlgorithms::NORMALIZATION:
				contrastNormalize(*intensity, output, 1.0);
				break;

			case ContrastAlgorithms::HISTOGRAM_EQUALIZATION:
				contrastEqualize(*intensity, output);
				break;

			case ContrastAlgorithms::CLAHE:
				contrastCLAHE(*intensity, output, 4.0, 8);
				break;

			default:
				// Other contrast enhancement types are not available in OpenCV.
				return;
				break;
		}

		// If original was a color frame, we have to mix the various
		// components back together into an RGB(A) image.
		if (output.channels() != 1) {
			cv::Mat YCrCbrgb;
			merge(yCrCbPlanes, 3, YCrCbrgb);

			CV_Assert(YCrCbrgb.type() == CV_8UC3);

			if (output.channels() == 4) {
				cv::Mat rgb;
				cvtColor(YCrCbrgb, rgb, cv::COLOR_YCrCb2RGB);

				CV_Assert(rgb.type() == CV_8UC3);

				// Restore original alpha.
				cv::Mat in[] = {rgb, rgbaAlpha};
				// rgb[0] -> rgba[0], rgb[1] -> rgba[1],
				// rgb[2] -> rgba[2], rgbaAlpha[0] -> rgba[3]
				int channelTransform[] = {0, 0, 1, 1, 2, 2, 3, 3};
				mixChannels(in, 2, &output, 1, channelTransform, 4);
			}
			else {
				cvtColor(YCrCbrgb, output, cv::COLOR_YCrCb2RGB);
			}
		}

		// Done.
		frame_out.commit();
	}

	void contrastNormalize(const cv::Mat &input, cv::Mat &output, float clipHistPercent) {
		CV_Assert((input.type() == CV_8UC1) && (output.type() == CV_8UC1));
		CV_Assert((clipHistPercent >= 0) && (clipHistPercent < 100));

		// O(x, y) = alpha * I(x, y) + beta, where alpha maximizes the range
		// (contrast) and beta shifts it so lowest is zero (brightness).
		double minValue;
		double maxValue;

		if (clipHistPercent == 0) {
			// Determine minimum and maximum values.
			minMaxLoc(input, &minValue, &maxValue);
		}
		else {
			// Calculate histogram.
			int histSize           = UINT8_MAX + 1;
			float hRange[]         = {0, static_cast<float>(histSize)};
			const float *histRange = {hRange};
			bool uniform           = true;
			bool accumulate        = false;

			cv::Mat hist;
			calcHist(&input, 1, nullptr, cv::Mat(), hist, 1, &histSize, &histRange, uniform, accumulate);

			// Calculate cumulative distribution from the histogram.
			for (int i = 1; i < histSize; i++) {
				hist.at<float>(i) += hist.at<float>(i - 1);
			}

			// Locate points that cut at required value.
			float total = hist.at<float>(histSize - 1);
			clipHistPercent *= (total / 100.0F); // Calculate absolute value from percent.
			clipHistPercent /= 2.0F;             // Left and right wings, so divide by two.

			// Locate left cut.
			minValue = 0;
			while (hist.at<float>(static_cast<int>(minValue)) < clipHistPercent) {
				minValue++;
			}

			// Locate right cut.
			maxValue = UINT8_MAX;
			while (hist.at<float>(static_cast<int>(maxValue)) >= (total - clipHistPercent)) {
				maxValue--;
			}
		}

		// Use min/max to calculate input range.
		double range = maxValue - minValue;

		// Calculate alpha (contrast).
		double alpha = (static_cast<double>(UINT8_MAX)) / range;

		// Calculate beta (brightness).
		double beta = -minValue * alpha;

		// Apply alpha and beta to pixels array.
		input.convertTo(output, -1, alpha, beta);
	}

	void contrastEqualize(const cv::Mat &input, cv::Mat &output) {
		CV_Assert((input.type() == CV_8UC1) && (output.type() == CV_8UC1));

		// Calculate histogram.
		int histSize           = UINT8_MAX + 1;
		float hRange[]         = {0, static_cast<float>(histSize)};
		const float *histRange = {hRange};
		bool uniform           = true;
		bool accumulate        = false;

		cv::Mat hist;
		calcHist(&input, 1, nullptr, cv::Mat(), hist, 1, &histSize, &histRange, uniform, accumulate);

		// Calculate cumulative distribution from the histogram.
		for (int i = 1; i < histSize; i++) {
			hist.at<float>(i) += hist.at<float>(i - 1);
		}

		// Total number of pixels. Must be the last value!
		float total = hist.at<float>(histSize - 1);

		// Smallest non-zero cumulative distribution value. Must be the first non-zero value!
		float min = 0;
		for (int i = 0; i < histSize; i++) {
			if (hist.at<float>(i) > 0) {
				min = hist.at<float>(i);
				break;
			}
		}

		// Calculate lookup table for histogram equalization.
		hist -= static_cast<double>(min);
		hist /= static_cast<double>(total - min);
		hist *= static_cast<double>(UINT8_MAX);

		// Apply lookup table to input image.
		int idx = 0;
		std::for_each(input.begin<uint8_t>(), input.end<uint8_t>(), [&hist, &output, &idx](const uint8_t &elem) {
			output.at<uint8_t>(idx++) = static_cast<uint8_t>(hist.at<float>(elem));
		});
	}

	void contrastCLAHE(const cv::Mat &input, cv::Mat &output, float clipLimit, int tilesGridSize) {
		CV_Assert((input.type() == CV_8UC1) && (output.type() == CV_8UC1));
		CV_Assert((clipLimit >= 0) && (clipLimit < 100));
		CV_Assert((tilesGridSize >= 1) && (tilesGridSize <= 64));

		// Apply the CLAHE algorithm to the intensity channel (luminance).
		cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
		clahe->setClipLimit(static_cast<double>(clipLimit));
		clahe->setTilesGridSize(cv::Size(tilesGridSize, tilesGridSize));
		clahe->apply(input, output);
	}

	void configUpdate() override {
		// Parse available choices into enum value.
		auto selectedContrastAlgo = config.get<dvCfgType::STRING>("contrastAlgorithm");

		if (selectedContrastAlgo == "histogram_equalization") {
			contrastAlgo = ContrastAlgorithms::HISTOGRAM_EQUALIZATION;
		}
		else if (selectedContrastAlgo == "clahe") {
			contrastAlgo = ContrastAlgorithms::CLAHE;
		}
		else {
			// Default choice.
			contrastAlgo = ContrastAlgorithms::NORMALIZATION;
		}
	}
};

registerModuleClass(FrameEnhancer)
