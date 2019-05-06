#ifndef DV_PROCESSING_FRAME_HPP
#define DV_PROCESSING_FRAME_HPP

#include "core.hpp"

namespace dv {

/**
 * Signum function
 * @tparam T The type of the numeric variable to get the sign of
 * @param val THe value to get the sign of
 * @return +1 if val > 0, -1 if val > 0, 0 if val == 0
 */
template<typename T> int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}

/**
 * Common accumulator class that allows to accumulate events into a frame.
 * The class is highly configurable to adapt to various use cases. This
 * is the preferred functionality for projecting events onto a frame.
 *
 * Accumulation of the events is performed on a floating point frame,
 * with every event contributing a fixed amount to the potential. Timestamps
 * of the last contributions are stored as well, to allow for a decay.
 */
class Accumulator {
public:
	/**
	 * Decay function to be used to decay the surface potential.
	 *
	 * * `NONE`: Do not decay at all. The potential can be reset manually
	 *    by calling the `clear` function
	 *
	 * * `LINEAR`: Perform a linear decay with  given slope. The linear decay goes
	 *    from currentpotential until the potential reaches the neutral potential
	 *
	 * * `EXPONENTIAL`: Exponential decay with time factor tau. The potential
	 *    eventually converges to zero.
	 *
	 * * `STEP`: Decay sharply to neutral potential after the given time.
	 *    Constant potential before.
	 */
	enum Decay { NONE = 0, LINEAR = 1, EXPONENTIAL = 2, STEP = 3 };

private:
	// input
	bool rectifyPolarity_;
	float eventContribution_;
	float maxPotential_;
	float neutralPotential_;
	float minPotential_;

	// decay
	Decay decayFunction_;
	double decayParam_;
	bool synchronousDecay_;

	// output
	cv::Size shape_;

	// state
	TimeMat decayTimeSurface_;
	cv::Mat potentialSurface_;
	time_t highestTime_;

	// internal use methods
	/**
	 * __INTERNAL_USE_ONLY__
	 * Decays the potential at coordinates x, y to the given time, respecting the
	 * decay function. Updates the time surface to the last decay.
	 * @param x The x coordinate of the value to be decayed
	 * @param y The y coordinate of the value to be decayed
	 * @param time The time to which the value should be decayed to.
	 */
	void decay(coord_t x, coord_t y, time_t time) {
		if (decayFunction_ == NONE) {
			return;
		}

		// normal handling for all the other functions
		time_t lastDecayTime = decayTimeSurface_.at(y, x);
		assert(lastDecayTime <= time);

		float lastPotential = potentialSurface_.at<float>(y, x);
		switch (decayFunction_) {
			case LINEAR: {
				potentialSurface_.at<float>(y, x)
					= (lastPotential >= neutralPotential_)
						  ? std::max(lastPotential - (float) ((double) (time - lastDecayTime) * decayParam_),
							  neutralPotential_)
						  : std::min(lastPotential + (float) ((double) (time - lastDecayTime) * decayParam_),
							  neutralPotential_);
				decayTimeSurface_.at(y, x) = time;
				break;
			}
			case EXPONENTIAL: {
				potentialSurface_.at<float>(y, x)
					= lastPotential * (float) std::exp(-((double) (time - lastDecayTime)) / decayParam_);
				decayTimeSurface_.at(y, x) = time;
				break;
			}
			case STEP: {
				potentialSurface_.at<float>(y, x)
					= (double) (time - lastDecayTime) > decayParam_ ? neutralPotential_ : lastPotential;
				break;
			}
			default: {
				potentialSurface_.at<float>(y, x) = lastPotential;
				break;
			}
		}
	}

	/**
	 * __INTERNAL_USE_ONLY__
	 * Contributes the effect of a single event onto the potential surface.
	 * @param x The x coordinate of where to contribute to
	 * @param y The y coordinate of where to contribute to
	 * @param polarity The polarity of the contribution
	 * @param time The event time at which the contribution happened
	 */
	void contribute(coord_t x, coord_t y, bool polarity, time_t time) {
		float lastPotential = potentialSurface_.at<float>(y, x);
		float contribution  = eventContribution_;
		if (!rectifyPolarity_ && !polarity) {
			contribution = -contribution;
		}

		float newPotential = std::min(std::max(lastPotential + contribution, minPotential_), maxPotential_);
		potentialSurface_.at<float>(y, x) = newPotential;

		if (decayFunction_ == STEP) {
			decayTimeSurface_.at(y, x) = time;
		}
	}

public:
	/**
	 * Silly default constructor. This generates an accumulator with zero size.
	 * An accumulator with zero size does not work. This constructor just exists
	 * to make it possible to default initialize an Accumulator to later redefine.
	 */
	Accumulator() = default;

	/**
	 * Accumulator constructor
	 * Creates a new Accumulator with the given params. By selecting the params
	 * the right way, the Accumulator can be used for a multitude of applications.
	 * The class also provides static factory functions that adjust the parameters
	 * for common use cases.
	 *
	 * @param size The size of the resulting frame. This must be at least the
	 * dimensions of the eventstream supposed to be added to the accumulator,
	 * otherwise this will result in memory errors.
	 * @param decayFunction The decay function to be used in this accumulator.
	 * The decay function is one of `NONE`, `LINEAR`, `EXPONENTIAL`, `STEP`. The
	 * function behave like their mathematical definitions, with LINEAR AND STEP
	 * going back to the `neutralPotential` over time, EXPONENTIAL going back to 0.
	 * @param decayParam The parameter to tune the decay function. The parameter has
	 * a different meaning depending on the decay function chosen:
	 * `NONE`: The parameter is ignored
	 * `LINEAR`: The paramaeter describes the (negative) slope of the linear function
	 * `EXPONENTIAL`: The parameter describes tau, by which the time difference is divided.
	 * @param synchronousDecay if set to true, all pixel values get decayed to the same time
	 * as soon as the frame is generated. If set to false, pixel values remain at the state
	 * they had when the last contribution came in.
	 * @param eventContribution The contribution a single event has onto the potential
	 * surface. This value gets interpreted positively or negatively depending on the
	 * event polarity
	 * @param maxPotential The upper cut-off value at which the potential surface
	 * is clipped
	 * @param neutralPotential The potential the decay function converges to over time.
	 * @param minPotential The lower cut-off value at which the potential surface
	 * is clipped
	 * @param rectifyPolarity Describes if the polarity of the events should be kept
	 * or ignored. If set to true, all events behave like positive events.
	 */
	Accumulator(const cv::Size &size, Accumulator::Decay decayFunction, double decayParam, bool synchronousDecay,
		float eventContribution, float maxPotential, float neutralPotential, float minPotential, bool rectifyPolarity) :
		rectifyPolarity_(rectifyPolarity),
		eventContribution_(eventContribution),
		maxPotential_(maxPotential),
		neutralPotential_(neutralPotential),
		minPotential_(minPotential),
		decayFunction_(decayFunction),
		decayParam_(decayParam),
		synchronousDecay_(synchronousDecay),
		shape_(size),
		decayTimeSurface_(TimeMat(size)),
		potentialSurface_(cv::Mat(size, CV_32F, (double) neutralPotential)),
		highestTime_(0) {
	}

	/**
	 * Accumulates all the events in the supplied packet and puts them onto the
	 * accumulation surface.
	 * @param packet The packet containing the events that should be
	 * accumulated.
	 */
	void accumulate(const EventStore &packet) {
		if (potentialSurface_.empty()) {
			return;
		}

		if (packet.isEmpty()) {
			return;
		}

		for (const Event &event : packet) {
			decay(event.x(), event.y(), event.timestamp());
			contribute(event.x(), event.y(), event.polarity(), event.timestamp());
		}
		highestTime_ = packet.getHighestTime();
	}

	/**
	 * Generates the accumulation frame (potential surface) at the provided time.
	 * The time must be higher or equal than the timestamp of the last event
	 * consumed by the accumulator. The function returns an OpenCV frame to work
	 * with. The frame has data type CV_32F.
	 * @param time T The time at which the frame should get generated
	 * @return An OpenCV frame with data type CV_32F containing the potential surface.
	 */
	cv::Mat generateFrame(time_t time) {
		if (synchronousDecay_) {
			assert(time >= highestTime_);
			for (int r = 0; r < potentialSurface_.rows; r++) {
				for (int c = 0; c < potentialSurface_.cols; c++) {
					decay(c, r, time);
				}
			}
		}
		cv::Mat out;
		potentialSurface_.copyTo(out);
		return out;
	}

	/**
	 * Generates the accumulation frame (potential surface) at the time of the
	 * last consumed event.
	 * The function returns an OpenCV frame to work
	 * with. The frame has data type CV_32F.
	 * @param time T The time at which the frame should get generated
	 * @return An OpenCV frame with data type CV_32F containing the potential surface.
	 */
	cv::Mat generateFrame() {
		return generateFrame(highestTime_);
	}

	/**
	 * Clears the potential surface by setting it to the neutral value.
	 * This function does not reset the time surface.
	 */
	void clear() {
		potentialSurface_ = cv::Mat(shape_, CV_32F, (double) neutralPotential_);
	}

	// setters
	/**
	 * If set to true, all events will incur a positive contribution to the
	 * potential surface
	 * @param rectifyPolarity The new value to set
	 */
	void setRectifyPolarity(bool rectifyPolarity) {
		Accumulator::rectifyPolarity_ = rectifyPolarity;
	}

	/**
	 * Contribution to the potential surface an event shall incur.
	 * This contribution is either counted positively (for positive events
	 * or when `rectifyPolatity` is set).
	 * @param eventContribution The contribution a single event shall incur
	 */
	void setEventContribution(float eventContribution) {
		Accumulator::eventContribution_ = eventContribution;
	}

	/**
	 * @param maxPotential the max potential at which the surface should be capped at
	 */
	void setMaxPotential(float maxPotential) {
		Accumulator::maxPotential_ = maxPotential;
	}

	/**
	 * @param neutralPotential The neutral potential to which the decay function should go.
	 * Exponential decay always goes to 0. The parameter is ignored there.
	 */
	void setNeutralPotential(float neutralPotential) {
		Accumulator::neutralPotential_ = neutralPotential;
	}

	/**
	 * @param minPotential the min potential at which the surface should be capped at
	 */
	void setMinPotential(float minPotential) {
		Accumulator::minPotential_ = minPotential;
	}

	/**
	 * @param decayFunction The decay function the module should use to perform the decay
	 */
	void setDecayFunction(Decay decayFunction) {
		Accumulator::decayFunction_ = decayFunction;
	}

	/**
	 * The decay param. This is slope for linear decay, tau for exponential decay
	 * @param decayParam The param to be used
	 */
	void setDecayParam(double decayParam) {
		Accumulator::decayParam_ = decayParam;
	}

	/**
	 * If set to true, all valued get decayed to the frame generation time at
	 * frame generation. If set to false, the values only get decayed on activity.
	 * @param synchronousDecay the new value for synchronoues decay
	 */
	void setSynchronousDecay(bool synchronousDecay) {
		Accumulator::synchronousDecay_ = synchronousDecay;
	}

	bool isRectifyPolarity() const {
		return rectifyPolarity_;
	}

	float getEventContribution() const {
		return eventContribution_;
	}

	float getMaxPotential() const {
		return maxPotential_;
	}

	float getNeutralPotential() const {
		return neutralPotential_;
	}

	float getMinPotential() const {
		return minPotential_;
	}

	Decay getDecayFunction() const {
		return decayFunction_;
	}

	double getDecayParam() const {
		return decayParam_;
	}

	const cv::Size &getShape() const {
		return shape_;
	}

	// static factories with predefined params
	/**
	 * Factory function to create a standard frame accumulator.
	 * The accumulator highlights positions where positive events happened white,
	 * positions with negative events black and positions with no events gray.
	 *
	 * There is no decay. The accumulator has to be cleared after every frame
	 * by the user.
	 * @param size The size (in pixels) of the Accumulator.
	 * @return An accumulator object with predefined settings.
	 */
	static Accumulator eventFrameAccumulator(const cv::Size &size) {
		return Accumulator(size, Accumulator::Decay::NONE, 0, false, .5, 1.0, .5, .0, false);
	}

	/**
	 * Exponential time decay accumulator factory method. Creates an accumulator
	 * that rectifies and sets every pixel where an event happens to 1 (maximum)
	 * value. After that, as long as there is no other event, the value of the
	 * pixel decays exponentially with `e^((t_{event} - t) / \tau)`
	 * @param size The size (in pixels) of the Accumulator
	 * @param tau The tau factor for the exponential decay.
	 * @return An accumulator object with predefined settings
	 */
	static Accumulator timeDecayFrameExponential(const cv::Size &size, double tau) {
		return Accumulator(size, Accumulator::Decay::EXPONENTIAL, tau, true, 1, 1, 0, 0, true);
	}

	/**
	 * Exponential time decay accumulator factory method. Creates an accumulator
	 * that rectifies and sets every pixel where an event happens to 1 (maximum)
	 * value. After that, as long as there is no other event, the value of the
	 * pixel decays exponentially with `1 * e^((t_{event} - t) / \tau)`
	 * @param size The size (in pixels) of the Accumulator
	 * @return An accumulator object with predefined settings
	 */
	static Accumulator timeDecayFrameExponential(const cv::Size &size) {
		return Accumulator::timeDecayFrameExponential(size, 1e6);
	}

	/**
	 * Linear time decay accumulator factory method. Creates an accumulator
	 * that rectifies and sets every pixel where an event happens to 1 (maximum)
	 * value. After that, as long as there is no other event, the value of the
	 * pixel decays linearly with `1 - ((t_{event} - t)/s))` where `s` denotes the slope.
	 * @param size The size (in pixels) of the Accumulator
	 * @param slope The slope of the linear decay
	 * @return An accumulator object with predefined settings
	 */
	static Accumulator timeDecayFrameLinear(const cv::Size &size, double slope) {
		return Accumulator(size, Accumulator::Decay::LINEAR, slope, true, 1, 1, 0, 0, true);
	}

	/**
	 * Linear time decay accumulator factory method. Creates an accumulator
	 * that rectifies and sets every pixel where an event happens to 1 (maximum)
	 * value. After that, as long as there is no other event, the value of the
	 * pixel decays linearly with `1 - ((t_{event} - t)/s))` where `s` denotes the slope.
	 * @param size The size (in pixels) of the Accumulator
	 * @return An accumulator object with predefined settings
	 */
	static Accumulator timeDecayFrameLinear(const cv::Size &size) {
		return Accumulator::timeDecayFrameLinear(size, 1e-6);
	}

	/**
	 * Factory method that returns an accumulator that can approximate a reconstruction
	 * of the grayscale frame.
	 * @param size The size (in pixels) of the accumulator
	 * @param decayFunction The decay function to be used
	 * @param decayParam The param for the decay function to be used
	 * @param eventContribution The contribution a single event should have on the potential
	 * @param neutralPotential The neutral potential that is initialized and to which the decay
	 * should converge
	 * @return An accumulator with predefined settings
	 */
	static Accumulator reconstructionFrame(
		const cv::Size &size, Decay decayFunction, double decayParam, float eventContribution, float neutralPotential) {
		return Accumulator(size, decayFunction, decayParam, false, eventContribution, 1, neutralPotential, 0, false);
	}

	/**
	 * Factory method that returns an accumulator that can approximate a reconstruction
	 * of the grayscale frame.
	 * @param size The size (in pixels) of the accumulator
	 * @param tau The param for the decay function to be used
	 * @param eventContribution The contribution a single event should have on the potential
	 * @return An accumulator with predefined settings
	 */
	static Accumulator reconstructionFrame(const cv::Size &size, double tau, float eventContribution) {
		return Accumulator::reconstructionFrame(size, Accumulator::Decay::EXPONENTIAL, tau, eventContribution, 0.0f);
	}

	/**
	 * Factory method that returns an accumulator that can approximate a reconstruction
	 * of the grayscale frame.
	 * @param size The size (in pixels) of the accumulator
	 * @return An accumulator with predefined settings
	 */
	static Accumulator reconstructionFrame(const cv::Size &size) {
		return Accumulator::reconstructionFrame(size, 1e6, 0.04f);
	}
};

/**
 * TimeSurface class that builds the surface of the occurences of the last
 * timestamps.
 */
class TimeSurface {
private:
	TimeMat timeSurface;

public:
	/**
	 * Creates a new time surface accumulator with the given size
	 * @param size The size (in pixels) for the time surface integrator
	 */
	explicit TimeSurface(const cv::Size &size) : timeSurface(TimeMat(size)){};
	void accumulate(const EventStore &store) {
		for (const Event &event : store) {
			timeSurface.at(event.y(), event.x()) = event.timestamp();
		}
	}

	/**
	 * Returns the current time surface
	 * @return An OpenCV Matrix containing the current time surface.
	 */
	const TimeMat &getTimeSurface() const {
		return timeSurface;
	}

	/**
	 * Returns the last event time at the given time
	 * @param x the x coordinate to obtain the last event time
	 * @param y the y coordinate to obtain the last event time
	 * @return The time at which the last event happened at this location.
	 * -1 if no event hasn't happened there yet.
	 */
	time_t at(coord_t x, coord_t y) const noexcept {
		return timeSurface.at(y, x);
	}
};

} // namespace dv

#endif // DV_PROCESSING_FRAME_HPP
