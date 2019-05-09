#include "dv-sdk/module.hpp"
#include "dv-sdk/processing.hpp"

class Accumulator : public dv::ModuleBase {
private:
	dv::EventStreamSlicer slicer;
	dv::Accumulator frameAccumulator;
	dv::slicejob_t sliceJob;

public:
	static const char *getDescription() {
		return "Accumulates events into a frame. Provides various configurations to tune the integration process";
	}

	static void addInputs(dv::InputDefinitionList &in) {
		in.addEventInput("events");
	}

	static void addOutputs(dv::OutputDefinitionList &out) {
		out.addFrameOutput("frames");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add("rectifyPolarity", dv::ConfigOption::boolOption("All events have positive contribution"));
		config.add("eventContribution",
			dv::ConfigOption::floatOption("The contribution of a single event", 0.04f, 0.0f, 1.0f));
		config.add("maxPotential", dv::ConfigOption::floatOption("Value at which to clip the integration", .3f));
		config.add("neutralPotential", dv::ConfigOption::floatOption("Value to which the decay tends over time", 0.0));
		config.add("minPotential", dv::ConfigOption::floatOption("Value at which to clip the integration", 0.0));
		config.add("decayFunction", dv::ConfigOption::listOption(
										"The decay function to be used", 2, {"None", "Linear", "Exponential", "Step"}));
		config.add(
			"decayParam", dv::ConfigOption::doubleOption(
							  "Slope for linear decay, tau for exponential decay, time for step decay", 1e6, 0, 1e10));
		config.add("synchronousDecay", dv::ConfigOption::boolOption("Decay at frame generation time"));
		config.add(
			"accumulationTime", dv::ConfigOption::intOption("Time in ms to accumulate events over", 33, 1, 1000));
	}

	void doPerFrameTime(const dv::EventStore &events) {
		frameAccumulator.accumulate(events);
		// generate frame
		auto frame = frameAccumulator.generateFrame();

		// make sure frame is in correct exposure and data type
		double scaleFactor
			= 255.0 / static_cast<double>(frameAccumulator.getMaxPotential() - frameAccumulator.getMinPotential());
		double shiftFactor = -static_cast<double>(frameAccumulator.getMinPotential()) * scaleFactor;
		cv::Mat correctedFrame;
		frame.convertTo(correctedFrame, CV_8U, scaleFactor, shiftFactor);

		// output
		outputs.getFrameOutput("frames") << correctedFrame;
	}

	Accumulator() : slicer(dv::EventStreamSlicer()) {
		outputs.getFrameOutput("frames").setup(inputs.getEventInput("events"));
		frameAccumulator = dv::Accumulator::reconstructionFrame(inputs.getEventInput("events").size());

		sliceJob = slicer.doEveryTimeInterval(config.getInt("accumulationTime") * 1000,
			std::function<void(const dv::EventStore &)>(
				std::bind(&Accumulator::doPerFrameTime, this, std::placeholders::_1)));
	}

	void run() override {
		slicer.addEventPacket(inputs.getEventInput("events").events());
	}

	static dv::Accumulator::Decay decayFromString(const std::string &name) {
		if (name == "Linear")
			return dv::Accumulator::Decay::LINEAR;
		if (name == "Exponential")
			return dv::Accumulator::Decay::EXPONENTIAL;
		if (name == "Step")
			return dv::Accumulator::Decay::STEP;
		return dv::Accumulator::Decay::NONE;
	}

	void configUpdate() override {
		frameAccumulator.setRectifyPolarity(config.getBool("rectifyPolarity"));
		frameAccumulator.setEventContribution(config.getFloat("eventContribution"));
		frameAccumulator.setMaxPotential(config.getFloat("maxPotential"));
		frameAccumulator.setNeutralPotential(config.getFloat("neutralPotential"));
		frameAccumulator.setMinPotential(config.getFloat("minPotential"));
		frameAccumulator.setDecayFunction(decayFromString(config.getString("decayFunction")));
		frameAccumulator.setDecayParam(config.getDouble("decayParam"));
		frameAccumulator.setSynchronousDecay(config.getBool("synchronousDecay"));
		slicer.modifyTimeInterval(sliceJob, config.getInt("accumulationTime") * 1000);
	}
};

registerModuleClass(Accumulator)
