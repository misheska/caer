#ifndef TYPES_HPP
#define TYPES_HPP

#include "dv-sdk/events/types.hpp"

#include <mutex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dv {

class Module;

namespace Types {

class TypeSystem {
private:
	std::vector<Type> systemTypes;
	std::unordered_map<uint32_t, std::vector<std::pair<const Module *, Type>>> userTypes;
	mutable std::mutex typesLock;

public:
	TypeSystem();

	void registerModuleType(const Module *m, const Type t);
	void unregisterModuleTypes(const Module *m);

	const Type getTypeInfo(std::string_view tIdentifier, const Module *m = nullptr) const;
	const Type getTypeInfo(const char *tIdentifier, const Module *m = nullptr) const;
	const Type getTypeInfo(uint32_t tId, const Module *m = nullptr) const;
};

} // namespace Types

} // namespace dv

#endif // TYPES_HPP
