#include "log.hpp"
#include "main.hpp"

static dv::MainData *glMainDataPtr = nullptr;

void dv::SDKLibInit(MainData *setMainDataPtr) {
	glMainDataPtr = setMainDataPtr;
}

const struct dvType dvTypeSystemGetInfoByIdentifier(const char *tIdentifier) {
	try {
		return (glMainDataPtr->typeSystem.getTypeInfo(tIdentifier, nullptr));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		// Return empty placeholder.
		return (dvType("NULL", "Empty placeholder type.", 0, nullptr, nullptr, nullptr, nullptr));
	}
}

const struct dvType dvTypeSystemGetInfoByID(uint32_t tId) {
	try {
		return (glMainDataPtr->typeSystem.getTypeInfo(tId, nullptr));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		// Return empty placeholder.
		return (dvType("NULL", "Empty placeholder type.", 0, nullptr, nullptr, nullptr, nullptr));
	}
}

static thread_local dv::LogBlock *loggerPtr = nullptr;

void dv::LoggerSet(dv::LogBlock *_logger) {
	loggerPtr = _logger;
}

void dv::LoggerVA(enum caer_log_level logLevel, const char *format, va_list argumentList) {
	auto localLogger = loggerPtr;

	if (localLogger == nullptr) {
		// System default logger.
		caerLogVA(logLevel, "DV-Runtime", format, argumentList);
	}
	else {
		// Specialized logger.
		auto localLogLevel = localLogger->logLevel.load(std::memory_order_relaxed);

		// Only log messages above the specified severity level.
		if (logLevel > localLogLevel) {
			return;
		}

		caerLogVAFull(static_cast<enum caer_log_level>(localLogLevel), logLevel, localLogger->logPrefix.c_str(), format,
			argumentList);
	}
}

void dvLog(enum caer_log_level logLevel, const char *format, ...) {
	va_list argumentList;
	va_start(argumentList, format);
	dv::LoggerVA(logLevel, format, argumentList);
	va_end(argumentList);
}
