//
// Created by najiji on 16.10.18.
//

#ifndef CAER_MODULE_HPP
#define CAER_MODULE_HPP

#include <iostream>
#include "caer-sdk/sshs/sshs.hpp"
#include "mainloop.h"

namespace caer {

    template<class T>
    class ModuleStaticDefinitions {
    public:
        static void configInit(sshsNode moduleNode) {
            UNUSED_ARGUMENT(moduleNode);
        }

        static bool init(caerModuleData moduleData) {
            try {
                new(moduleData->moduleState) T();
            } catch (const std::exception &e) {
                std::cerr << e.what();
                std::cerr << "Could not initialize Module";
                return false;
            }
            return true;
        }

        static void run(caerModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
            ((T *) moduleData->moduleState)->run(in, out);
        }

        static void exit(caerModuleData moduleData) {
            ((T *) moduleData->moduleState)->~T();
        }

        static void config(caerModuleData moduleData) {
            ((T *) moduleData->moduleState)->config();
        }

        static const struct caer_module_functions functions;
        static const struct caer_module_info info;
    };

    template<class T> const caer_module_functions ModuleStaticDefinitions<T>::functions = {
            .moduleConfigInit = &ModuleStaticDefinitions<T>::configInit,
            .moduleInit       = &ModuleStaticDefinitions<T>::init,
            .moduleRun        = &ModuleStaticDefinitions<T>::run,
            .moduleConfig     = &ModuleStaticDefinitions<T>::config,
            .moduleExit       = &ModuleStaticDefinitions<T>::exit,
            .moduleReset      = NULL
    };

    template<class T> const caer_module_info ModuleStaticDefinitions<T>::info = {
            .version             = 1,
            .name                = T::name,
            .description         = T::description,
            .type                = T::type,
            .memSize             = sizeof(T),
            .functions           = &functions,
            .inputStreamsSize    = T::inputStreams == NULL ? 0: CAER_EVENT_STREAM_IN_SIZE(T::inputStreams),
            .inputStreams        = T::inputStreams,
            .outputStreamsSize   = T::outputStreams == NULL ? 0 : CAER_EVENT_STREAM_OUT_SIZE(T::outputStreams),
            .outputStreams       = T::outputStreams
    };


    class BaseModule {
    public:
        virtual void config() = 0;
        virtual void run(caerEventPacketContainer in, caerEventPacketContainer *out) = 0;
    };




    class PolarityEventConsumer : public BaseModule {
    public:
        static const caer_event_stream_in inputStreams[];
        static const caer_event_stream_out* outputStreams;
        static const caer_module_type type = CAER_MODULE_OUTPUT;

        virtual void handleEvent(const libcaer::events::PolarityEvent &event) = 0;

        void config() override {
        };

        void run(caerEventPacketContainer in, caerEventPacketContainer *out) override {
            UNUSED_ARGUMENT(out);

            auto inPacketContainer = libcaer::events::EventPacketContainer(in, false);
            auto framePackage = std::static_pointer_cast<libcaer::events::FrameEventPacket>(inPacketContainer.findEventPacketByType(FRAME_EVENT));
            auto polarityPackage = std::static_pointer_cast<libcaer::events::PolarityEventPacket>(inPacketContainer.findEventPacketByType(POLARITY_EVENT));

            if (polarityPackage && !polarityPackage->empty()) {
                for (const auto &event : *polarityPackage) {
                    if (!event.isValid()) {
                        continue;
                    }
                    handleEvent(event);
                }
            }
        }
    };

    const caer_event_stream_in PolarityEventConsumer::inputStreams[1] = {
            { .type = POLARITY_EVENT, .number = 1, .readOnly = true }
    };

    const caer_event_stream_out* PolarityEventConsumer::outputStreams = NULL;







}


#endif //CAER_MODULE_HPP
