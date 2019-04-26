#ifndef DV_SDK_FRAME_HPP
#define DV_SDK_FRAME_HPP

#include "frame_base.hpp"
#include "wrappers.hpp"

// Allow disabling of OpenCV requirement.
#ifndef DV_API_OPENCV_SUPPORT
#	define DV_API_OPENCV_SUPPORT 1
#endif

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
#	include <opencv2/core.hpp>
#	include <opencv2/core/utility.hpp>
#endif

namespace dv {

template<> class InputDataWrapper<Frame> {
private:
	using NativeType = typename Frame::NativeTableType;

	std::shared_ptr<const NativeType> ptr;
#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	std::shared_ptr<const cv::Mat> matPtr;
#endif

public:
	InputDataWrapper(std::shared_ptr<const NativeType> p) : ptr(std::move(p)) {
#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
		// Use custom deleter to bind life-time of main data 'ptr' to OpenCV 'matPtr'.
		if (ptr) {
			matPtr = std::shared_ptr<const cv::Mat>{new cv::Mat(ptr->sizeY, ptr->sizeX, static_cast<int>(ptr->format),
														const_cast<uint8_t *>(ptr->pixels.data())),
				[ptr = ptr](const cv::Mat *mp) { delete mp; }};
		}
#endif
	}

	explicit operator bool() const noexcept {
		return (ptr.get() != nullptr);
	}

	std::shared_ptr<const NativeType> getBasePointer() const noexcept {
		return (ptr);
	}

	const NativeType &operator*() const noexcept {
		return (*(ptr.get()));
	}

	const NativeType *operator->() const noexcept {
		return (ptr.get());
	}

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	/**
	 * Return a read-only OpenCV Mat representing this frame.
	 *
	 * @return a read-only OpenCV Mat pointer
	 */
	std::shared_ptr<const cv::Mat> getMatPointer() const noexcept {
		return (matPtr);
	}
#endif
};

template<> class OutputDataWrapper<Frame> {
private:
	using NativeType = typename Frame::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputDataWrapper(NativeType *p, dvModuleData m, const std::string &n) : ptr(p), moduleData(m), name(n) {
	}

	void commit() noexcept {
		// Ignore frames with no pixels.
		if ((ptr == nullptr) || ptr->pixels.empty()) {
			return;
		}

		dvModuleOutputCommit(moduleData, name.c_str());

		// Update with next object, in case we continue to use this.
		auto typedObject = dvModuleOutputAllocate(moduleData, name.c_str());
		if (typedObject == nullptr) {
			// Actual errors will write a log message and return null.
			// No data just returns null. So if null we simply forward that.
			ptr = nullptr;
		}
		else {
			ptr = static_cast<NativeType *>(typedObject->obj);
		}
	}

	explicit operator bool() const noexcept {
		return (ptr != nullptr);
	}

	NativeType *getBasePointer() noexcept {
		return (ptr);
	}

	const NativeType *getBasePointer() const noexcept {
		return (ptr);
	}

	NativeType &operator*() noexcept {
		return (*ptr);
	}

	const NativeType &operator*() const noexcept {
		return (*ptr);
	}

	NativeType *operator->() noexcept {
		return (ptr);
	}

	const NativeType *operator->() const noexcept {
		return (ptr);
	}

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	/**
	 * Return an OpenCV Mat representing this frame.
	 * Please note the actual backing memory comes from the Frame->pixels vector.
	 * If the underlying vector changes, due to resize or commit, this Mat becomes
	 * invalid and should not be used anymore.
	 * Also, for this to work, the pixels array must already have its memory allocated
	 * such that Frame.pixels.size() >= (Frame.sizeX * Frame.sizeY * Frame.numChannels).
	 * If that is not the case, a std::out_of_range exception is thrown.
	 *
	 * @return an OpenCV Mat
	 */
	cv::Mat getMat() {
		// Verify size.
		auto channels = (static_cast<int>(ptr->format) / 8) + 1;
		if (ptr->pixels.size() < static_cast<size_t>(ptr->sizeX * ptr->sizeY * channels)) {
			throw std::out_of_range(
				"getMat(): Frame.pixels.size() smaller than (Frame.sizeX * Frame.sizeY * Frame.numChannels).");
		}

		return (cv::Mat{ptr->sizeY, ptr->sizeX, static_cast<int>(ptr->format), ptr->pixels.data()});
	}

	/**
	 * Return a read-only OpenCV Mat representing this frame.
	 * Please note the actual backing memory comes from the Frame->pixels vector.
	 * If the underlying vector changes, due to resize or commit, this Mat becomes
	 * invalid and should not be used anymore.
	 * Also, for this to work, the pixels array must already have its memory allocated
	 * such that Frame.pixels.size() >= (Frame.sizeX * Frame.sizeY * Frame.numChannels).
	 * If that is not the case, a std::out_of_range exception is thrown.
	 *
	 * @return a read-only OpenCV Mat
	 */
	const cv::Mat getMat() const {
		// Verify size.
		auto channels = (static_cast<int>(ptr->format) / 8) + 1;
		auto requiredSize = static_cast<size_t>(ptr->sizeX * ptr->sizeY * channels);
		if (ptr->pixels.size() < requiredSize) {
			// In case the buffer does not correspond to the specified sizes, we resize the buffer
			ptr->pixels.resize(requiredSize);
		}

		return (cv::Mat{ptr->sizeY, ptr->sizeX, static_cast<int>(ptr->format), ptr->pixels.data()});
	}

	void commitMat(const cv::Mat &mat) {
		ptr->sizeX = static_cast<int16_t>(mat.cols);
		ptr->sizeY = static_cast<int16_t>(mat.rows);

		switch (mat.channels()) {
			case 1:
				ptr->format = dv::FrameFormat::GRAY;
				break;
			case 3:
				ptr->format = dv::FrameFormat::BGR;
				break;
			case 4:
				ptr->format = dv::FrameFormat::BGRA;
				break;
			default:
				throw std::out_of_range((boost::format("Unsupported number of channels in OpenCV Mat: %d") % mat.channels()).str());
		}
		cv::Mat outMat = getMat();
		switch (mat.type() % 8) {
			case CV_8U:
				mat.copyTo(outMat);
				break;
			case CV_8S:
				mat.convertTo(outMat, CV_8U, 1, 128);
				break;
			case CV_16U:
				mat.convertTo(outMat, CV_8U, 1./256., 0);
				break;
			case CV_16S:
				mat.convertTo(outMat, CV_8U, 1./256., 128);
				break;
			case CV_32F:
			case CV_64F:
				mat.convertTo(outMat, CV_8U, 255, 0);
				break;
			default:
				throw std::out_of_range((boost::format("Unsupported OpenCV data type: %d") % mat.type()).str());
		}
		commit();
	}

	OutputDataWrapper<dv::Frame>& operator<<(const cv::Mat &mat) {
		commitMat(mat);
		return *this;
	}

#endif
};

} // namespace dv

#endif // DV_SDK_FRAME_HPP
