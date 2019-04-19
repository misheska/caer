#include "../log.hpp"
#include "../main.hpp"

const dv::SDKLibFunctionPointers *dv::glLibFuncPtr = nullptr;

void dv::SDKLibInit(const dv::SDKLibFunctionPointers *setLibFuncPtr) {
	dv::glLibFuncPtr = setLibFuncPtr;
}

struct dvType dvTypeSystemGetInfoByIdentifier(const char *tIdentifier) {
	try {
		return (dv::glLibFuncPtr->getTypeInfoCharString(tIdentifier, nullptr));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		// Return empty placeholder.
		return (dv::glLibFuncPtr->getTypeInfoIntegerID(dv::Types::nullId, nullptr));
	}
}

struct dvType dvTypeSystemGetInfoByID(uint32_t tId) {
	try {
		return (dv::glLibFuncPtr->getTypeInfoIntegerID(tId, nullptr));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		// Return empty placeholder.
		return (dv::glLibFuncPtr->getTypeInfoIntegerID(dv::Types::nullId, nullptr));
	}
}

static thread_local const dv::LogBlock *loggerPtr = nullptr;

void dv::LoggerSet(const dv::LogBlock *_logger) {
	loggerPtr = _logger;
}

const dv::LogBlock *dv::LoggerGet() {
	return (loggerPtr);
}

void dvLog(enum caer_log_level level, const char *format, ...) {
	auto localLogger = loggerPtr;

	va_list argumentList;
	va_start(argumentList, format);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	if (localLogger == nullptr) {
		// System default logger.
		caerLogVA(level, "Runtime", format, argumentList);
	}
	else {
		// Specialized logger.
		auto localLogLevel = localLogger->logLevel.load(std::memory_order_relaxed);

		// Only log messages above the specified severity level.
		if (level > localLogLevel) {
			return;
		}

		caerLogVAFull(static_cast<enum caer_log_level>(localLogLevel), level, localLogger->logPrefix.c_str(), format,
			argumentList);
	}
#pragma GCC diagnostic pop

	va_end(argumentList);
}
