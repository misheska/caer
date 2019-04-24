#include <dv-sdk/data/frame_base.hpp>
#include "dv-sdk/data/event.hpp"
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
        out.emplace_back("frame", dv::Frame::identifier);
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
        int sizeX = inputs.getInfo("events").get<dv::CfgType::INT>("sizeX");
        int sizeY = inputs.getInfo("events").get<dv::CfgType::INT>("sizeY");
        size = cv::Size(sizeX, sizeY);
        frameAccumulator = dv::Accumulator::reconstructionFrame(size);
    }


    static dv::Accumulator::Decay decayFromString(const std::string &name) {
        if (name == "Linear") return dv::Accumulator::Decay::LINEAR;
        if (name == "Exponential") return dv::Accumulator::Decay::EXPONENTIAL;
        if (name == "Step") return dv::Accumulator::Decay::STEP;
        return dv::Accumulator::Decay::NONE;
    }

    void run() override {
        // update the configs
        frameAccumulator.setRectifyPolarity(config.get<dv::CfgType::BOOL>("rectifyPolarity"));
        frameAccumulator.setEventContribution(config.get<dv::CfgType::FLOAT>("eventContribution"));
        frameAccumulator.setMaxPotential(config.get<dv::CfgType::FLOAT>("maxPotential"));
        frameAccumulator.setNeutralPotential(config.get<dv::CfgType::FLOAT>("neutralPotential"));
        frameAccumulator.setMinPotential(config.get<dv::CfgType::FLOAT>("minPotential"));
        frameAccumulator.setDecayFunction(decayFromString(config.get<dv::CfgType::STRING>("decayFunction")));
        frameAccumulator.setDecayParam(config.get<dv::CfgType::DOUBLE>("decayParam"));
        frameAccumulator.setSynchronousDecay(config.get<dv::CfgType::BOOL>("synchronousDecay"));

        // integrate frame
        frameAccumulator.accumulate(inputs.get<dv::EventPacket>("events"));

        // generate frame
        auto frame = frameAccumulator.generateFrame();

        // make sure frame is in correct exposure and data type
        double scaleFactor = 255./((double)frameAccumulator.getMaxPotential() - (double)frameAccumulator.getMinPotential());
        double shiftFactor = -(double)frameAccumulator.getMinPotential() * scaleFactor;
        cv::Mat correctedFrame;
        frame.convertTo(correctedFrame, CV_8U, scaleFactor, shiftFactor);

        // output
        outputs.get<dv::Frame>("frame");

    }


};

registerModuleClass(Accumulator)