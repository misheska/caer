#ifndef CAER_LOG_HPP
#define CAER_LOG_HPP

#include <iostream>
#include <boost/format.hpp>
#include "module.h"


/**
 * Header file that includes the c++ wrapper for the caer logging facilities.
 */
namespace caer {

    /**
     * caer custom log end type. Can be piped into the log to commit it.
     * Eg. `log.info << "Hello " << "world" << caer::logEnd`.
     */
    struct logEndType {};
    logEndType logEnd {};

    /**
     * LogStream class. To be instantiated only by caer::Logger. Handles the
     * actual logging and provides functions to stream logging data in as well as
     * to format logging strings.
     * @tparam L The log level for which this stream should be configured.
     */
    template <caer_log_level L>
    class LogStream {
    private:
        /**
         * Pointer to the caer moduleData of the module this logger
         * is compiled for. Gets set upon construction.
         */
        dvModuleData moduleData_ = nullptr;

        /**
         * internal string stream that gets used to assemble the log message.
         */
        std::ostringstream stream_;

    public:
        /**
         * Constructs a new LogStream. A log stream is an object to which one can
         * stream data to be logged to. It also provides functions for formatting
         * log messages.
         * @param moduleData a pointer to the caer moduleData struct of the module
         * this is compiled to.
         */
        explicit LogStream(dvModuleData moduleData) : moduleData_(moduleData) {};

        /**
         * Logs the given argument value.
         * @tparam T the type of the argument value
         * @param val the value to be logged
         */
        template <class T>
        void operator()(const T &val) {
            write(val);
            commit();
        }

        /**
         * Appends the given argument value to the current log message.
         * To commit a log message (write it out and flush), one can stream in
         * `caer::logEnd` or `std::endl`.
         * @tparam T The type of the value to be appended to the log
         * @param val The value to be appended to the log
         * @return A reference to the stream, to stream in further values
         */
        template <class T>
        LogStream& operator<<(const T &val) {
            write(val);
            return *this;
        }

        /**
         * Commits a log message upon reception of an object of the type of
         * `std::endl`. Committing a message means writing it out and flushing
         * the buffer.
         * @return a reference to the log stream for the reception of further
         * values to be logged.
         */
        LogStream& operator <<(std::ostream& (*)(std::ostream&))
        {
            commit();
            return *this;
        }

        /**
         * Commits a log message upon reception of `caer::logEnd`.
         * Committing a message means writing it out and flushing
         * the buffer.
         * @return a reference to the log stream for the reception of further
         * values to be logged.
         */
        LogStream& operator <<(logEndType) {
            commit();
            return *this;
        }

        /**
         * Appends the given argument value to the current log message.
         * To commit a log message (write it out and flush), one can use the function
         * `commit()`
         * @tparam T The type of the value to be appended to the log
         * @param val The value to be appended to the log
         */
        template <class T>
        void write(T &val) {
            stream_ << val;
        }

        /**
         * Commits the current log message. Committing means writing the message
         * out and flushing the buffer.
         */
        void commit() {
            if (moduleData_) {
                caerModuleLog(moduleData_, L, "%s", stream_.str().c_str());
            } else {
                caerLog(L, "[UNKNOWN SUBSYSTEM]", "%s", stream_.str().c_str());
            }
            flush();
        }

        /**
         * Flushes the buffer without writing out the message.
         */
        void flush() {
            stream_.str(std::string());
            stream_.clear();
        }

        /**
         * Formats the given format string with the given arguments and writes it
         * to the log. Takes any `boost::format` format string.
         * @tparam Args The types of the arguments
         * @param fmt The format string
         * @param args The argument values to be logged.
         */
        template<typename... Args>
        void format(const std::string &fmt, const Args&... args) {
            boost::format f(fmt);
            std::initializer_list<char> {(static_cast<void>(f % args), char{}) ...};
            this->operator()(boost::str(f));
        }
    };

    /**
     * Logger class. This class provides logging streams for different log
     * levels to log to.
     */
    class Logger {
    public:
        LogStream<CAER_LOG_DEBUG> debug;
        LogStream<CAER_LOG_INFO> info;
        LogStream<CAER_LOG_WARNING> warning;
        LogStream<CAER_LOG_ERROR> error;
        LogStream<CAER_LOG_CRITICAL> critical;

        /**
         * Constructor. Initializes log streams for the different log levels.
         * @param moduleData pointer to the caer moduleData struct.
         */
        explicit Logger(dvModuleData moduleData) :
        debug(LogStream<CAER_LOG_DEBUG>(moduleData)),
        info(LogStream<CAER_LOG_INFO>(moduleData)),
        warning(LogStream<CAER_LOG_WARNING>(moduleData)),
        error(LogStream<CAER_LOG_ERROR>(moduleData)),
        critical(LogStream<CAER_LOG_CRITICAL>(moduleData))  {};
    };
}

#endif //CAER_LOG_HPP
