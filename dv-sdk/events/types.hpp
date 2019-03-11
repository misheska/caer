#ifndef TYPES_HPP
#define TYPES_HPP

#	include "dv-sdk/events/flatbuffers/flatbuffers.h"

namespace dv::Types {

using PackFuncPtr   = flatbuffers::uoffset_t (*)(flatbuffers::FlatBufferBuilder *toBuffer, const void *fromObject);
using UnpackFuncPtr = void (*)(void *toObject, const void *fromBuffer);

struct Type {
	uint32_t id;
	char identifier[5];
	PackFuncPtr pack;
	UnpackFuncPtr unpack;

	Type(const char *_identifier, PackFuncPtr _pack, UnpackFuncPtr _unpack) : pack(_pack), unpack(_unpack) {
		// Four character identifier, NULL terminated.
		identifier[0] = _identifier[0];
		identifier[1] = _identifier[1];
		identifier[2] = _identifier[2];
		identifier[3] = _identifier[3];
		identifier[4] = '\0';

		// Transform into 32bit integer for fast comparison.
		// Does not have to be endian-neutral, only used internally.
		id = *(reinterpret_cast<uint32_t *>(identifier));
	}
};

} // namespace dv::Types


#endif // TYPES_HPP
