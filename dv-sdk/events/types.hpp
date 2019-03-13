#ifndef DV_SDK_TYPES_HPP
#define DV_SDK_TYPES_HPP

#include "dv-sdk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*dvTypePackFuncPtr)(void *toBuffer, const void *fromObject);
typedef void (*dvTypeUnpackFuncPtr)(void *toObject, const void *fromBuffer);

struct dvType {
	uint32_t id;
	char identifier[5];
	dvTypePackFuncPtr pack;
	dvTypeUnpackFuncPtr unpack;

#ifdef __cplusplus
	dvType(const char *_identifier, dvTypePackFuncPtr _pack, dvTypeUnpackFuncPtr _unpack) :
		pack(_pack),
		unpack(_unpack) {
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

	bool operator==(const dvType &rhs) const noexcept {
		return ((id == rhs.id) && (pack == rhs.pack) && (unpack == rhs.unpack));
	}

	bool operator!=(const dvType &rhs) const noexcept {
		return (!operator==(rhs));
	}
#endif
};

bool dvTypesRegisterType(const dvType t);
bool dvTypesUnregisterType(const dvType t);

#ifdef __cplusplus
}

static_assert(std::is_standard_layout_v<dvType>, "dvType is not standard layout");

#	include "dv-sdk/events/flatbuffers/flatbuffers.h"

namespace dv::Types {

using PackFuncPtr   = dvTypePackFuncPtr;
using UnpackFuncPtr = dvTypeUnpackFuncPtr;
using Type          = dvType;

template<typename FBType, typename ObjectAPIType> static uint32_t Packer(void *toBuffer, const void *fromObject) {
	return (FBType::Pack(*(static_cast<flatbuffers::FlatBufferBuilder *>(toBuffer)),
		static_cast<const ObjectAPIType *>(fromObject), nullptr)
				.o);
}
template<typename FBType, typename ObjectAPIType> static void Unpacker(void *toObject, const void *fromBuffer) {
	FBType::UnPackToFrom(static_cast<ObjectAPIType *>(toObject), static_cast<const FBType *>(fromBuffer), nullptr);
}

} // namespace dv::Types

#endif

#endif // DV_SDK_TYPES_HPP
