//
// Created by najiji on 16.01.19.
//

#ifndef CAER_LOG_HPP
#define CAER_LOG_HPP

#include "module.h"

namespace caer {
    /**
     * Static pointer to the modules caer data. Gets set by a setter function.
     * When not set, logging is performed anonymously
     */
    static caerModuleData moduleData_ = nullptr;


    template <caer_log_level L>
    class LogStream {
    private:
        static inline void logImpl(const std::string &message) {
            if (moduleData_) {
                caerModuleLog(moduleData_, L, "%s", message.c_str());
            } else {
                caerLog(L, "[UNKNOWN SUBSYSTEM]", "%s", message.c_str());
            }
        }

    public:


        void operator()(const std::string &message) const {
            logImpl(message);
        }

//        void bla(const std::string &message) const {
//            logImpl(message);
//        }

    };

    class Logger {
    public:
        static void _setModuleData(caerModuleData moduleData) {
            moduleData_ = moduleData;
        }

        LogStream<CAER_LOG_DEBUG> debug;
        LogStream<CAER_LOG_INFO> info;
        LogStream<CAER_LOG_WARNING> warning;
        LogStream<CAER_LOG_ERROR> error;
        LogStream<CAER_LOG_CRITICAL> critical;
    };
}

#endif //CAER_LOG_HPP
