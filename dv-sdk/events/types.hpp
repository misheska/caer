#ifndef DV_SDK_TYPES_HPP
#define DV_SDK_TYPES_HPP

#include "dv-sdk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*dvTypePackFuncPtr)(void *toBuffer, const void *fromObject);
typedef void (*dvTypeUnpackFuncPtr)(void *toObject, const void *fromBuffer);
typedef void *(*dvTypeConstructPtr)(size_t sizeOfObject);
typedef void (*dvTypeDestructPtr)(void *object);

struct dvType {
	uint32_t id;
	char identifier[5];
	size_t sizeOfType;
	dvTypePackFuncPtr pack;
	dvTypeUnpackFuncPtr unpack;
	dvTypeConstructPtr construct;
	dvTypeDestructPtr destruct;

#ifdef __cplusplus
	dvType(const char *_identifier, size_t _sizeOfType, dvTypePackFuncPtr _pack, dvTypeUnpackFuncPtr _unpack,
		dvTypeConstructPtr _construct, dvTypeDestructPtr _destruct) :
		sizeOfType(_sizeOfType),
		pack(_pack),
		unpack(_unpack),
		construct(_construct),
		destruct(_destruct) {
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
		return ((id == rhs.id) && (sizeOfType == rhs.sizeOfType) && (pack == rhs.pack) && (unpack == rhs.unpack)
				&& (construct == rhs.construct) && (destruct == rhs.destruct));
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

using Type          = dvType;
using PackFuncPtr   = dvTypePackFuncPtr;
using UnpackFuncPtr = dvTypeUnpackFuncPtr;
using ConstructPtr  = dvTypeConstructPtr;
using DestructPtr   = dvTypeDestructPtr;

template<typename FBType, typename ObjectAPIType> static uint32_t Packer(void *toBuffer, const void *fromObject) {
	return (FBType::Pack(*(static_cast<flatbuffers::FlatBufferBuilder *>(toBuffer)),
		static_cast<const ObjectAPIType *>(fromObject), nullptr)
				.o);
}
template<typename FBType, typename ObjectAPIType> static void Unpacker(void *toObject, const void *fromBuffer) {
	FBType::UnPackToFrom(static_cast<ObjectAPIType *>(toObject), static_cast<const FBType *>(fromBuffer), nullptr);
}

template<typename ObjectAPIType> static void *Constructor(size_t sizeOfObject) {
	ObjectAPIType *obj = static_cast<ObjectAPIType *>(malloc(sizeOfObject));
	if (obj == nullptr) {
		throw std::bad_alloc();
	}

	new (obj) ObjectAPIType{};

	return (obj);
}
template<typename ObjectAPIType> static void Destructor(void *object) {
	ObjectAPIType *obj = static_cast<ObjectAPIType *>(object);
	if (obj == nullptr) {
		throw std::invalid_argument("object ptr is null");
	}

	obj->~ObjectAPIType();

	free(obj);
}

template<typename FBType, typename ObjectAPIType> static Type makeTypeDefinition() {
	static_assert(std::is_standard_layout_v<ObjectAPIType>, "ObjectAPIType is not standard layout");

	return (Type{FBType::rootIdentifier(), sizeof(ObjectAPIType), &Packer<FBType, ObjectAPIType>,
		&Unpacker<FBType, ObjectAPIType>, &Constructor<ObjectAPIType>, &Destructor<ObjectAPIType>});
}

} // namespace dv::Types

#endif

#endif // DV_SDK_TYPES_HPP
