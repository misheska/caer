#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/module.hpp"

#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

class FrameStatistics : public dv::ModuleBase {
public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("frames", dv::Frame::identifier, false);
	}

	static void addOutputs(std::vector<dv::OutputDefinition> &out) {
		out.emplace_back("histograms", dv::Frame::identifier);
	}

	static const char *getDescription() {
		return ("Display statistics on frames (histogram).");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add("numBins", dv::ConfigOption::intOption("Number of bins in which to divide values up.", 256, 4, 256));
	}

	FrameStatistics() {
		// Wait for input to be ready. All inputs, once they are up and running, will
		// have a valid sourceInfo node to query, especially if dealing with data.
		// Allocate map using info from sourceInfo.
		auto info = inputs.getInfo("frames");
		if (!info) {
			throw std::runtime_error("Frames input not ready, upstream module not running.");
		}

		// Populate frame output info node. Must have generated statistics histogram frame
		// maximum size. Max size is 256 x 128 due to max number of bins being 256.
		auto outInfoNode = outputs.getInfoNode("histograms");
		outInfoNode.create<dvCfgType::INT>(
			"sizeX", 256, {256, 256}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Maximum X axis size of frame.");
		outInfoNode.create<dvCfgType::INT>(
			"sizeY", 128, {128, 128}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Maximum Y axis size of frame.");
	}

	void run() override {
		auto frame_in = inputs.get<dv::Frame>("frames");
		auto hist_out = outputs.get<dv::Frame>("histograms");

		auto numBins = config.get<dvCfgType::INT>("numBins");

		hist_out->sizeX     = static_cast<int16_t>(numBins);
		hist_out->sizeY     = static_cast<int16_t>(numBins / 2);
		hist_out->format    = dv::FrameFormat::GRAY;
		hist_out->timestamp = frame_in->timestamp; // Only set main timestamp.
		hist_out->pixels.resize(
			static_cast<size_t>(hist_out->sizeX * hist_out->sizeY)); // Allocate memory (1 channel, grayscale).

		// Calculate histogram, full uint8 range.
		const float range[]    = {0, 256};
		const float *histRange = {range};

		cv::Mat hist;
		cv::calcHist(frame_in.getMatPointer().get(), 1, nullptr, cv::Mat(), hist, 1, &numBins, &histRange, true, false);

		// Generate histogram image, with N x N/2 pixels.
		auto histImage = hist_out.getMat();

		// Normalize the result to [0, histImage.rows].
		cv::normalize(hist, hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());

		// Draw the histogram.
		for (int i = 1; i < histImage.cols; i++) {
			cv::line(histImage, cv::Point(i - 1, histImage.rows - cvRound(hist.at<float>(i - 1))),
				cv::Point(i, histImage.rows - cvRound(hist.at<float>(i))), cv::Scalar(255, 255, 255), 1, cv::LINE_8, 0);
		}

		// Send histogram out.
		hist_out.commit();
	}
};

registerModuleClass(FrameStatistics)
