#include "../module.hpp"

void dvModuleRegisterType(dvModuleData moduleData, const struct dvType type) {
	auto module = static_cast<dv::Module *>(moduleData);

	try {
		module->registerType(type);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

void dvModuleRegisterOutput(dvModuleData moduleData, const char *name, const char *typeName) {
	auto module = static_cast<dv::Module *>(moduleData);

	try {
		module->registerOutput(name, typeName);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

void dvModuleRegisterInput(dvModuleData moduleData, const char *name, const char *typeName, bool optional) {
	auto module = static_cast<dv::Module *>(moduleData);

	try {
		module->registerInput(name, typeName, optional);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
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
