#include "types.hpp"

#include "dv-sdk/data/event_base.hpp"
#include "dv-sdk/data/frame_base.hpp"
#include "dv-sdk/data/imu_base.hpp"
#include "dv-sdk/data/trigger_base.hpp"

#include <stdexcept>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

namespace dv::Types {

static inline void makeTypeNode(const Type &t, dvCfg::Node n) {
	auto typeNode = n.getRelativeNode(std::string(t.identifier) + "/");

	typeNode.create<dvCfgType::STRING>(
		"description", t.description, {0, 2000}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type description.");

	typeNode.create<dvCfgType::LONG>("size", static_cast<int64_t>(t.sizeOfType), {0, INT64_MAX},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type size.");
}

TypeSystem::TypeSystem() {
	auto systemTypesNode = dvCfg::GLOBAL.getNode("/system/types/system/");

	// Initialize system types. These are always available due to
	// being compiled into the core.
	auto evtType = makeTypeDefinition<EventPacket>("Array of events (polarity ON/OFF).");
	systemTypes.push_back(evtType);
	makeTypeNode(evtType, systemTypesNode);

	auto frmType = makeTypeDefinition<Frame>("Standard frame (8-bit image).");
	systemTypes.push_back(frmType);
	makeTypeNode(frmType, systemTypesNode);

	auto imuType = makeTypeDefinition<IMUPacket>("Inertial Measurement Unit data samples.");
	systemTypes.push_back(imuType);
	makeTypeNode(imuType, systemTypesNode);

	auto trigType = makeTypeDefinition<TriggerPacket>("External triggers and special signals.");
	systemTypes.push_back(trigType);
	makeTypeNode(trigType, systemTypesNode);
}

void TypeSystem::registerModuleType(const Module *m, const Type &t) {
	std::scoped_lock lock(typesLock);

	// Register user type. Rules:
	// a) cannot have same identifier as a system type
	// b) if same type registered multiple times, just
	// add multiple times. Modules will register on
	// load/dlopen, and unregister on unload/dlclose.
	// As such, a pack/unpack will always be present if
	// any module that uses such a type is loaded at least
	// once. It is assumed types with same identifier are
	// equal or fully compatible.
	if (findIfBool(
			systemTypes.cbegin(), systemTypes.cend(), [&t](const auto &sysType) { return (t.id == sysType.id); })) {
		throw std::invalid_argument("Already present as a system type.");
	}

	// Not a system type. Check if this module already registered
	// this type before.
	if (userTypes.count(t.id)
		&& findIfBool(userTypes[t.id].cbegin(), userTypes[t.id].cend(),
			[m](const auto &userType) { return (m == userType.first); })) {
		throw std::invalid_argument("User type already registered for this module.");
	}

	userTypes[t.id].emplace_back(m, t);

	auto userTypesNode = dvCfg::GLOBAL.getNode("/system/types/user/");
	makeTypeNode(t, userTypesNode);
}

void TypeSystem::unregisterModuleTypes(const Module *m) {
	std::scoped_lock lock(typesLock);

	// Remove all types registered to this module.
	for (auto &type : userTypes) {
		auto &vec = type.second;
		auto pos  = std::find_if(vec.cbegin(), vec.cend(), [m](const auto &userType) { return (m == userType.first); });

		if (pos != vec.cend()) {
			vec.erase(pos);
		}
	}

	// Cleanup empty vectors.
	for (auto it = userTypes.begin(); it != userTypes.end();) {
		if (it->second.empty()) {
			// Empty vector means no survivors of this type, so we can remove
			// it from the global registry too.
			uint32_t id       = it->first;
			const char *idStr = reinterpret_cast<const char *>(&id);
			std::string identifier{idStr, 4};

			auto userTypesNode = dvCfg::GLOBAL.getNode("/system/types/user/");
			userTypesNode.getRelativeNode(identifier + "/").removeNode();

			it = userTypes.erase(it);
		}
		else {
			++it;
		}
	}
}

const Type TypeSystem::getTypeInfo(std::string_view tIdentifier, const Module *m) const {
	if (tIdentifier.size() != 4) {
		throw std::invalid_argument("Identifier must be 4 characters long.");
	}

	uint32_t id = *(reinterpret_cast<const uint32_t *>(tIdentifier.data()));

	return (getTypeInfo(id, m));
}

const Type TypeSystem::getTypeInfo(const char *tIdentifier, const Module *m) const {
	if (strlen(tIdentifier) != 4) {
		throw std::invalid_argument("Identifier must be 4 characters long.");
	}

	uint32_t id = *(reinterpret_cast<const uint32_t *>(tIdentifier));

	return (getTypeInfo(id, m));
}

const Type TypeSystem::getTypeInfo(uint32_t tId, const Module *m) const {
	std::scoped_lock lock(typesLock);

	// Search for type, first in system then user types.
	auto sysPos = std::find_if(
		systemTypes.cbegin(), systemTypes.cend(), [tId](const auto &sysType) { return (tId == sysType.id); });

	if (sysPos != systemTypes.cend()) {
		// Found.
		return (*sysPos);
	}

	if (m == nullptr) {
		throw std::invalid_argument("For user type lookups, the related module must be defined.");
	}

	if (userTypes.count(tId) > 0) {
		auto userPos = std::find_if(userTypes.at(tId).cbegin(), userTypes.at(tId).cend(),
			[m](const auto &userType) { return (m == userType.first); });

		if (userPos != userTypes.at(tId).cend()) {
			// Found.
			return (userPos->second);
		}
	}

	// Not found.
	throw std::out_of_range("Type not found in type system.");
}

} // namespace dv::Types
