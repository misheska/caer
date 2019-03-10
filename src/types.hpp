#ifndef TYPES_HPP
#define TYPES_HPP

#include "dv-sdk/events/flatbuffers/flatbuffers.h"

#include "dv-sdk/utils.h"

#include <array>
#include <map>

namespace dv::types {

using PackFuncPtr = flatbuffers::Offset<void> (*)(
    flatbuffers::FlatBufferBuilder &, const void *, const flatbuffers::rehasher_function_t *);
using UnpackFuncPtr = void (*)(void *, const void *, const flatbuffers::resolver_function_t *);

struct Type {
    uint32_t id;
    char identifier[5];
    PackFuncPtr pack;
    UnpackFuncPtr unpack;

    Type(const char *_identifier, PackFuncPtr _pack, UnpackFuncPtr _unpack) : pack(_pack), unpack(_unpack) {
        // Four character identifier, NULL terminated.
        memcpy(identifier, _identifier, 4);
        identifier[4] = '\0';

        // Transform into 32bit integer for fast comparison.
        // Does not have to be endian-neutral, only used internally.
        id = *(reinterpret_cast<uint32_t *>(identifier));
    }
};

class TypeSystem {
private:
    std::vector<Type> systemTypes;
    std::map<uint32_t, std::vector<Type>> userTypes;

public:
    TypeSystem();

    void registerType(const Type t);
    void unregisterType(const Type t);
};

} // namespace dv::types

#endif // TYPES_HPP
