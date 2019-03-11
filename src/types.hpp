#ifndef TYPES_HPP
#define TYPES_HPP

#include "dv-sdk/events/types.hpp"

#include "dv-sdk/utils.h"

#include <array>
#include <map>
#include <mutex>

namespace dv::Types {

class TypeSystem {
private:
	std::vector<Type> systemTypes;
	std::map<uint32_t, std::vector<Type>> userTypes;
	std::recursive_mutex typesLock;

public:
	TypeSystem();

	void registerType(const Type t);
	void unregisterType(const Type t);

	Type getTypeInfo(const char *tIdentifier);
	Type getTypeInfo(uint32_t tId);
};

} // namespace dv::Types

#endif // TYPES_HPP
