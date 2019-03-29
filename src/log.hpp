#ifndef LOG_HPP_
#define LOG_HPP_

#include "dv-sdk/utils.h"

#include <atomic>
#include <string>

#define DV_LOG_FILE_NAME ".dv-logger.txt"

namespace dv {

struct LogBlock {
	std::string logPrefix;
	std::atomic_int32_t logLevel;
};

void LoggerInit(void);

// Part of SDK due to dvLog() having to be part of it.
void LoggerSet(LogBlock *logger);
LogBlock *LoggerGet();
void LoggerVA(enum caer_log_level logLevel, const char *format, va_list argumentList);

} // namespace dv

#endif /* LOG_HPP_ */
