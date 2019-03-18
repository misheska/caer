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
	const char *identifier;
	const char *description;
	size_t sizeOfType;
	dvTypePackFuncPtr pack;
	dvTypeUnpackFuncPtr unpack;
	dvTypeConstructPtr construct;
	dvTypeDestructPtr destruct;

#ifdef __cplusplus
	dvType(const char *_identifier, const char *_description, size_t _sizeOfType, dvTypePackFuncPtr _pack,
		dvTypeUnpackFuncPtr _unpack, dvTypeConstructPtr _construct, dvTypeDestructPtr _destruct) :
		identifier(_identifier),
		description(_description),
		sizeOfType(_sizeOfType),
		pack(_pack),
		unpack(_unpack),
		construct(_construct),
		destruct(_destruct) {
		if (identifier == nullptr) {
			throw std::invalid_argument("Type identifier must be defined.");
		}

		if (strlen(identifier) != 4) {
			throw std::invalid_argument("Type identifier must be exactly four characters long.");
		}

		if ((description == nullptr) || (strlen(description) == 0)) {
			throw std::invalid_argument("Type description must be defined.");
		}

		if (sizeOfType == 0) {
			throw std::invalid_argument("Type size must be bigger than zero.");
		}

		// Transform into 32bit integer for fast comparison.
		// Does not have to be endian-neutral, only used internally.
		id = *(reinterpret_cast<const uint32_t *>(identifier));
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

struct dvTypedArray {
	uint32_t typeId;
	size_t typeSize;
	size_t elemSize;
	void *elem;
};

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

template<typename FBType, typename ObjectAPIType>
static Type makeTypeDefinition(const char *identifier, const char *description) {
	static_assert(std::is_standard_layout_v<ObjectAPIType>, "ObjectAPIType is not standard layout");

	return (Type{identifier, description, sizeof(ObjectAPIType), &Packer<FBType, ObjectAPIType>,
		&Unpacker<FBType, ObjectAPIType>, &Constructor<ObjectAPIType>, &Destructor<ObjectAPIType>});
}

} // namespace dv::Types

#endif

#endif // DV_SDK_TYPES_HPP
