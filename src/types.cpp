#include "types.hpp"

#include "dv-sdk/events/frame8.hpp"
#include "dv-sdk/events/polarity.hpp"

#include <boost/format.hpp>
#include <stdexcept>

namespace dv::types {

TypeSystem::TypeSystem() {
    // Initialize system types. These are always available due to
    // being compiled into the core.
    systemTypes.emplace_back("SPEC", nullptr, nullptr);

    systemTypes.emplace_back(PolarityPacketIdentifier(), reinterpret_cast<PackFuncPtr>(&PolarityPacket::Pack),
        reinterpret_cast<UnpackFuncPtr>(&PolarityPacket::UnPackToFrom));

    systemTypes.emplace_back(Frame8PacketIdentifier(), reinterpret_cast<PackFuncPtr>(&Frame8Packet::Pack),
        reinterpret_cast<UnpackFuncPtr>(&Frame8Packet::UnPackToFrom));

    systemTypes.emplace_back("IMU9", nullptr, nullptr);
}

void TypeSystem::registerType(const Type t) {
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

} // namespace dv::types
