//
// Created by najiji on 16.01.19.
//

#ifndef CAER_LOG_HPP
#define CAER_LOG_HPP

#include "module.h"

namespace caer {
    template <caer_log_level L>
    class LogStream {
    private:
        caerModuleData moduleData_ = nullptr;
        inline void logImpl(const std::string &message) {
            if (moduleData_) {
                caerModuleLog(moduleData_, L, "%s", message.c_str());
            } else {
                caerLog(L, "[UNKNOWN SUBSYSTEM]", "%s", message.c_str());
            }
        }

    public:

        LogStream(caerModuleData moduleData) : moduleData_(moduleData) {};

        void operator()(const std::string &message) const {
            logImpl(message);
        }
    };

    class Logger {
    public:
        LogStream<CAER_LOG_DEBUG> debug;
        LogStream<CAER_LOG_INFO> info;
        LogStream<CAER_LOG_WARNING> warning;
        LogStream<CAER_LOG_ERROR> error;
        LogStream<CAER_LOG_CRITICAL> critical;

        Logger(caerModuleData moduleData) :
        debug(LogStream<CAER_LOG_DEBUG>(moduleData)),
        info(LogStream<CAER_LOG_INFO>(moduleData)),
        warning(LogStream<CAER_LOG_WARNING>(moduleData)),
        error(LogStream<CAER_LOG_ERROR>(moduleData)),
        critical(LogStream<CAER_LOG_CRITICAL>(moduleData))  {};
    };
}

#endif //CAER_LOG_HPP
