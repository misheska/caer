#ifndef LOG_HPP_
#define LOG_HPP_

#include "dv-sdk/utils.h"

#include <boost/format.hpp>

#define DV_LOG_FILE_NAME ".dv-logger.txt"

struct logBlock {
	std::string logPrefix;
	std::atomic_int32_t logLevel;
};

void dvLogInit(void);

template<typename... Args> void dvLog(libcaer::log::logLevel logLevel, const std::string &format, Args &&... args);

void dvLog(libcaer::log::logLevel logLevel, const boost::format &format);

#endif /* LOG_HPP_ */
