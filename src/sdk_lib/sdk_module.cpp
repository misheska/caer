#include "dv-sdk/module.h"

#include "../main.hpp"

void dvModuleRegisterType(dvModuleData moduleData, const struct dvType type) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		dv::glLibFuncPtr->registerType(module, type);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

void dvModuleRegisterOutput(dvModuleData moduleData, const char *name, const char *typeName) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		dv::glLibFuncPtr->registerOutput(module, name, typeName);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

void dvModuleRegisterInput(dvModuleData moduleData, const char *name, const char *typeName, bool optional) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		dv::glLibFuncPtr->registerInput(module, name, typeName, optional);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

struct dvTypedObject *dvModuleOutputAllocate(dvModuleData moduleData, const char *name) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		return (dv::glLibFuncPtr->outputAllocate(module, name));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		return (nullptr);
	}
}

void dvModuleOutputCommit(dvModuleData moduleData, const char *name) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		dv::glLibFuncPtr->outputCommit(module, name);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

const struct dvTypedObject *dvModuleInputGet(dvModuleData moduleData, const char *name) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		return (dv::glLibFuncPtr->inputGet(module, name));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		return (nullptr);
	}
}

void dvModuleInputDismiss(dvModuleData moduleData, const char *name, const struct dvTypedObject *data) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		dv::glLibFuncPtr->inputDismiss(module, name, data);
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());
	}
}

dvConfigNode dvModuleOutputGetInfoNode(dvModuleData moduleData, const char *name) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		return (static_cast<dvConfigNode>(dv::glLibFuncPtr->outputGetInfoNode(module, name)));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		return (nullptr);
	}
}

dvConfigNodeConst dvModuleInputGetInfoNode(dvModuleData moduleData, const char *name) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		return (static_cast<dvConfigNodeConst>(dv::glLibFuncPtr->inputGetInfoNode(module, name)));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		return (nullptr);
	}
}

bool dvModuleInputIsConnected(dvModuleData moduleData, const char *name) {
	auto module = reinterpret_cast<dv::Module *>(moduleData);

	try {
		return (dv::glLibFuncPtr->inputIsConnected(module, name));
	}
	catch (const std::exception &ex) {
		dv::Log(dv::logLevel::CRITICAL, "%s", ex.what());

		return (false);
	}
}
