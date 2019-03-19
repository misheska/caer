#ifndef LOG_HPP_
#define LOG_HPP_

#include "dv-sdk/utils.h"

#include <boost/format.hpp>

#define DV_LOG_FILE_NAME ".dv-logger.txt"

namespace dv {

using logLevel = libcaer::log::logLevel;

struct LogBlock {
	std::string logPrefix;
	std::atomic_int32_t logLevel;
};

void LoggerInit(void);

void LoggerSet(LogBlock *logger);

void Log(logLevel logLevel, const char *format, ...);
void Log(logLevel logLevel, const std::string format, ...);
void Log(logLevel logLevel, const boost::format &format);

} // namespace dv

#endif /* LOG_HPP_ */
