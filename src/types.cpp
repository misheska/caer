#include "types.hpp"

#include "dv-sdk/events/frame8.hpp"
#include "dv-sdk/events/polarity.hpp"

#include <stdexcept>

namespace dv::Types {

TypeSystem::TypeSystem() {
	// Initialize system types. These are always available due to
	// being compiled into the core.
	systemTypes.push_back(
		makeTypeDefinition<PolarityPacket, PolarityPacketT>(PolarityPacketIdentifier(), "Polarity ON/OFF events."));

	systemTypes.push_back(
		makeTypeDefinition<Frame8Packet, Frame8PacketT>(Frame8PacketIdentifier(), "Standard frames (8-bit images)."));
}

void TypeSystem::registerModuleType(const Module *m, const Type t) {
	std::lock_guard<std::recursive_mutex> lock(typesLock);

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
}

void TypeSystem::unregisterModuleTypes(const Module *m) {
	std::lock_guard<std::recursive_mutex> lock(typesLock);

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
	std::lock_guard<std::recursive_mutex> lock(typesLock);

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
