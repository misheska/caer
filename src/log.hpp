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
void LoggerSet(const LogBlock *logger);
const LogBlock *LoggerGet();

} // namespace dv

#endif /* LOG_HPP_ */
