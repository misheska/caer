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

void TypeSystem::registerType(const Type t) {
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
			systemTypes.cbegin(), systemTypes.cend(), [&t](const Type &sysType) { return (t.id == sysType.id); })) {
		throw std::invalid_argument("Already present as system type.");
	}

	// Not a system type. Add to list.
	userTypes[t.id].push_back(t);
}

void TypeSystem::unregisterType(const Type t) {
	std::lock_guard<std::recursive_mutex> lock(typesLock);

	if (userTypes.count(t.id) == 0) {
		// Non existing type, error!
		throw std::invalid_argument("Type does not exist.");
	}

	auto &vec = userTypes[t.id];
	auto pos  = std::find(vec.cbegin(), vec.cend(), t);

	if (pos != vec.cend()) {
		vec.erase(pos);
	}

	// Remove empty types.
	if (vec.empty()) {
		userTypes.erase(t.id);
	}
}

Type TypeSystem::getTypeInfo(const char *tIdentifier) const {
	uint32_t id = *(reinterpret_cast<const uint32_t *>(tIdentifier));

	return (getTypeInfo(id));
}

Type TypeSystem::getTypeInfo(uint32_t tId) const {
	std::lock_guard<std::recursive_mutex> lock(typesLock);

	// Search for type, first in system then user types.
	auto sysPos
		= std::find_if(systemTypes.cbegin(), systemTypes.cend(), [tId](const Type &t) { return (t.id == tId); });

	if (sysPos != systemTypes.cend()) {
		// Found.
		return (*sysPos);
	}

	if (userTypes.count(tId) > 0) {
		return (userTypes.at(tId).at(0));
	}

	// Not fund.
	throw std::out_of_range("Type not found in type system.");
}

} // namespace dv::Types
