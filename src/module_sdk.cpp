#include "log.hpp"
#include "main.hpp"
#include "module.hpp"

static dv::MainData *glMainDataPtr = nullptr;

void dv::SDKLibInit(MainData *setMainDataPtr) {
	glMainDataPtr = setMainDataPtr;
}

static thread_local dv::LogBlock *loggerPtr = nullptr;

void dv::LoggerSet(dv::LogBlock *_logger) {
	loggerPtr = _logger;
}

void dv::LoggerVA(enum caer_log_level logLevel, const char *format, va_list argumentList) {
	auto localLogger = loggerPtr;

	if (localLogger == nullptr) {
		// System default logger.
		caerLogVAFull(caerLogLevelGet(), logLevel, "DV-Runtime", format, argumentList);
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

void dvModuleRegisterType(dvConfigNode moduleNode, const struct dvType type) {
	std::string moduleName(dvConfigNodeGetName(moduleNode));

	{
		std::scoped_lock lock(glMainDataPtr->modulesLock);

		try {
			glMainDataPtr->modules.at(moduleName)->registerType(type);
		}
		catch (const std::exception &ex) {
			dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
		}
	}
}

void dvModuleRegisterOutput(dvConfigNode moduleNode, const char *name, const char *typeName) {
	std::string moduleName(dvConfigNodeGetName(moduleNode));

	{
		std::scoped_lock lock(glMainDataPtr->modulesLock);

		try {
			glMainDataPtr->modules.at(moduleName)->registerOutput(name, typeName);
		}
		catch (const std::exception &ex) {
			dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
		}
	}
}

void dvModuleRegisterInput(dvConfigNode moduleNode, const char *name, const char *typeName, bool optional) {
	std::string moduleName(dvConfigNodeGetName(moduleNode));

	{
		std::scoped_lock lock(glMainDataPtr->modulesLock);

		try {
			glMainDataPtr->modules.at(moduleName)->registerInput(name, typeName, optional);
		}
		catch (const std::exception &ex) {
			dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
		}
	}
}

struct dvTypedObject *dvModuleOutputAllocate(dvModuleData moduleData, const char *name) {
	auto module = static_cast<dv::Module *>(moduleData);

	try {
		return (module->outputAllocate(name));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		return (nullptr);
	}
}

void dvModuleOutputCommit(dvModuleData moduleData, const char *name) {
	auto module = static_cast<dv::Module *>(moduleData);

	try {
		module->outputCommit(name);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

const struct dvTypedObject *dvModuleInputGet(dvModuleData moduleData, const char *name) {
	auto module = static_cast<dv::Module *>(moduleData);

	try {
		return (module->inputGet(name));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		return (nullptr);
	}
}

void dvModuleInputDismiss(dvModuleData moduleData, const char *name, const struct dvTypedObject *data) {
	auto module = static_cast<dv::Module *>(moduleData);

	try {
		module->inputDismiss(name, data);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}
