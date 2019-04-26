#include "dv-sdk/module.hpp"
#include "dv-sdk/processing.hpp"

class Accumulator : public dv::ModuleBase {

private:
    cv::Size size;
    dv::Accumulator frameAccumulator;

public:
    static const char* getDescription() {
        return "Accumulates events into a frame. Provides various configurations to tune the integration process";
    }

    static void addInputs(std::vector<dv::InputDefinition> &in) {
        in.emplace_back("events", dv::EventPacket::identifier, false);
    }

    static void addOutputs(std::vector<dv::OutputDefinition> &out) {
        out.emplace_back("frames", dv::Frame::identifier);
    }

    static void getConfigOptions(dv::RuntimeConfig &config) {
        config.add("rectifyPolarity", dv::ConfigOption::boolOption("All events have positive contribution"));
        config.add("eventContribution", dv::ConfigOption::floatOption("The contribution of a single event", 0.04f));
        config.add("maxPotential", dv::ConfigOption::floatOption("Value at which to clip the integration", 1.0));
        config.add("neutralPotential", dv::ConfigOption::floatOption("Value to which the decay tends over time", 0.0));
        config.add("minPotential", dv::ConfigOption::floatOption("Value at which to clip the integration", 0.0));
        config.add("decayFunction", dv::ConfigOption::listOption("The decay function to be used",
                                                                2, {"None", "Linear", "Exponential", "Step"}));
        config.add("decayParam", dv::ConfigOption::doubleOption("Slope for linear decay, tau for exponential decay, time for step decay", 1e6, 0, 1e10));
        config.add("synchronousDecay", dv::ConfigOption::boolOption("Decay at frame generation time"));
    }


    Accumulator() {
        int sizeX = inputs.getEventInput("events").sizeX();
        int sizeY = inputs.getEventInput("events").sizeY();
        size = cv::Size(sizeX, sizeY);
        frameAccumulator = dv::Accumulator::reconstructionFrame(size);
    }


    void run() override {
        // integrate frame
        frameAccumulator.accumulate(inputs.getEventInput("events").data());

        // generate frame
        auto frame = frameAccumulator.generateFrame();

        // make sure frame is in correct exposure and data type
        double scaleFactor = 255./((double)frameAccumulator.getMaxPotential() - (double)frameAccumulator.getMinPotential());
        double shiftFactor = -(double)frameAccumulator.getMinPotential() * scaleFactor;
        cv::Mat correctedFrame;
        frame.convertTo(correctedFrame, CV_8U, scaleFactor, shiftFactor);

        // output
        outputs.getFrame("frame") << correctedFrame;
    }

    static dv::Accumulator::Decay decayFromString(const std::string &name) {
        if (name == "Linear") return dv::Accumulator::Decay::LINEAR;
        if (name == "Exponential") return dv::Accumulator::Decay::EXPONENTIAL;
        if (name == "Step") return dv::Accumulator::Decay::STEP;
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
    }
};

registerModuleClass(Accumulator)