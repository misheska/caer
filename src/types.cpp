#include "types.hpp"

#include "dv-sdk/events/frame8.hpp"
#include "dv-sdk/events/polarity.hpp"

#include <boost/format.hpp>
#include <stdexcept>

namespace dv::Types {

TypeSystem::TypeSystem() {
	// Initialize system types. These are always available due to
	// being compiled into the core.
	systemTypes.emplace_back(PolarityPacketIdentifier(), &Packer<PolarityPacket, PolarityPacketT>,
		&Unpacker<PolarityPacket, PolarityPacketT>);

	systemTypes.emplace_back(
		Frame8PacketIdentifier(), &Packer<Frame8Packet, Frame8PacketT>, &Unpacker<Frame8Packet, Frame8PacketT>);
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
		boost::format msg = boost::format("Failed to register type %s, already a system type.") % t.identifier;

		caerLog(CAER_LOG_ERROR, "Types", msg.str().c_str());
		throw std::invalid_argument(msg.str().c_str());
	}

	// Not a system type. Add to list.
	userTypes[t.id].push_back(t);
}

void TypeSystem::unregisterType(const Type t) {
	std::lock_guard<std::recursive_mutex> lock(typesLock);

	if (userTypes.count(t.id) == 0) {
		// Non existing type, error!
		boost::format msg = boost::format("Failed to unregister type %s, type does not exist.") % t.identifier;

		caerLog(CAER_LOG_ERROR, "Types", msg.str().c_str());
		throw std::invalid_argument(msg.str().c_str());
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

Type TypeSystem::getTypeInfo(const char *tIdentifier) {
	uint32_t id = *(reinterpret_cast<const uint32_t *>(tIdentifier));

	return (getTypeInfo(id));
}

Type TypeSystem::getTypeInfo(uint32_t tId) {
	std::lock_guard<std::recursive_mutex> lock(typesLock);

	// Search for type, first in system then user types.
	auto sysPos
		= std::find_if(systemTypes.cbegin(), systemTypes.cend(), [tId](const Type &t) { return (t.id == tId); });

	if (sysPos != systemTypes.cend()) {
		// Found.
		return (*sysPos);
	}

	if (userTypes.count(tId) > 0) {
		return (userTypes[tId].at(0));
	}

	// Not fund.
	throw std::out_of_range("Type not found in type system.");
}

} // namespace dv::Types
