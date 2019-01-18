//
// Created by najiji on 16.01.19.
//

#ifndef CAER_LOG_HPP
#define CAER_LOG_HPP

#include <iostream>
#include <boost/format.hpp>
#include "module.h"

namespace caer {
    template <caer_log_level L>
    class LogStream {
    private:
        caerModuleData moduleData_ = nullptr;
        std::ostringstream stream;

    public:
        LogStream(caerModuleData moduleData) : moduleData_(moduleData) {};

        template <class T>
        void operator()(const T &val) {
            write(val);
            commit();
        }

        template <class T>
        LogStream& operator<<(const T &val) {
            write(val);
            return *this;
        }

        LogStream& operator <<(std::ostream& (*)(std::ostream&))
        {
            commit();
            return *this;
        }

        template <class T>
        void write(T &val) {
            stream << val;
        }

        void commit() {
            if (moduleData_) {
                caerModuleLog(moduleData_, L, "%s", stream.str().c_str());
            } else {
                caerLog(L, "[UNKNOWN SUBSYSTEM]", "%s", stream.str().c_str());
            }
            flush();
        }

        void flush() {
            stream.str(std::string());
            stream.clear();
        }

        template<typename... Args>
        void format(const std::string &fmt, const Args&... args) {
            boost::format f(fmt);
            std::initializer_list<char> {(static_cast<void>(f % args), char{}) ...};
            this->operator()(boost::str(f));
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
