// automatically generated by the FlatBuffers compiler, do not modify

#ifndef FLATBUFFERS_GENERATED_DVCONFIGACTIONDATA_DV_H_
#define FLATBUFFERS_GENERATED_DVCONFIGACTIONDATA_DV_H_

#include "dv-sdk/data/cstring.hpp"
#include "dv-sdk/data/flatbuffers/flatbuffers.h"

namespace dv {

struct ConfigActionData;
struct ConfigActionDataT;

bool operator==(const ConfigActionDataT &lhs, const ConfigActionDataT &rhs);

inline const flatbuffers::TypeTable *ConfigActionDataTypeTable();

enum class ConfigAction : int8_t {
	ERROR              = 0,
	NODE_EXISTS        = 1,
	ATTR_EXISTS        = 2,
	GET_CHILDREN       = 3,
	GET_ATTRIBUTES     = 4,
	GET_TYPE           = 5,
	GET_RANGES         = 6,
	GET_FLAGS          = 7,
	GET_DESCRIPTION    = 8,
	GET                = 9,
	PUT                = 10,
	ADD_MODULE         = 11,
	REMOVE_MODULE      = 12,
	ADD_PUSH_CLIENT    = 13,
	REMOVE_PUSH_CLIENT = 14,
	PUSH_MESSAGE_NODE  = 15,
	PUSH_MESSAGE_ATTR  = 16,
	DUMP_TREE          = 17,
	DUMP_TREE_NODE     = 18,
	DUMP_TREE_ATTR     = 19,
	GET_CLIENT_ID      = 20,
	MIN                = ERROR,
	MAX                = GET_CLIENT_ID
};

inline const ConfigAction (&EnumValuesConfigAction())[21] {
	static const ConfigAction values[]
		= {ConfigAction::ERROR, ConfigAction::NODE_EXISTS, ConfigAction::ATTR_EXISTS, ConfigAction::GET_CHILDREN,
			ConfigAction::GET_ATTRIBUTES, ConfigAction::GET_TYPE, ConfigAction::GET_RANGES, ConfigAction::GET_FLAGS,
			ConfigAction::GET_DESCRIPTION, ConfigAction::GET, ConfigAction::PUT, ConfigAction::ADD_MODULE,
			ConfigAction::REMOVE_MODULE, ConfigAction::ADD_PUSH_CLIENT, ConfigAction::REMOVE_PUSH_CLIENT,
			ConfigAction::PUSH_MESSAGE_NODE, ConfigAction::PUSH_MESSAGE_ATTR, ConfigAction::DUMP_TREE,
			ConfigAction::DUMP_TREE_NODE, ConfigAction::DUMP_TREE_ATTR, ConfigAction::GET_CLIENT_ID};
	return values;
}

inline const char *const *EnumNamesConfigAction() {
	static const char *const names[] = {"ERROR", "NODE_EXISTS", "ATTR_EXISTS", "GET_CHILDREN", "GET_ATTRIBUTES",
		"GET_TYPE", "GET_RANGES", "GET_FLAGS", "GET_DESCRIPTION", "GET", "PUT", "ADD_MODULE", "REMOVE_MODULE",
		"ADD_PUSH_CLIENT", "REMOVE_PUSH_CLIENT", "PUSH_MESSAGE_NODE", "PUSH_MESSAGE_ATTR", "DUMP_TREE",
		"DUMP_TREE_NODE", "DUMP_TREE_ATTR", "GET_CLIENT_ID", nullptr};
	return names;
}

inline const char *EnumNameConfigAction(ConfigAction e) {
	if (e < ConfigAction::ERROR || e > ConfigAction::GET_CLIENT_ID)
		return "";
	const size_t index = static_cast<int>(e);
	return EnumNamesConfigAction()[index];
}

enum class ConfigType : int8_t {
	UNKNOWN = -1,
	BOOL    = 0,
	INT     = 1,
	LONG    = 2,
	FLOAT   = 3,
	DOUBLE  = 4,
	STRING  = 5,
	MIN     = UNKNOWN,
	MAX     = STRING
};

inline const ConfigType (&EnumValuesConfigType())[7] {
	static const ConfigType values[] = {ConfigType::UNKNOWN, ConfigType::BOOL, ConfigType::INT, ConfigType::LONG,
		ConfigType::FLOAT, ConfigType::DOUBLE, ConfigType::STRING};
	return values;
}

inline const char *const *EnumNamesConfigType() {
	static const char *const names[] = {"UNKNOWN", "BOOL", "INT", "LONG", "FLOAT", "DOUBLE", "STRING", nullptr};
	return names;
}

inline const char *EnumNameConfigType(ConfigType e) {
	if (e < ConfigType::UNKNOWN || e > ConfigType::STRING)
		return "";
	const size_t index = static_cast<int>(e) - static_cast<int>(ConfigType::UNKNOWN);
	return EnumNamesConfigType()[index];
}

enum class ConfigNodeEvents : int8_t { NODE_ADDED = 0, NODE_REMOVED = 1, MIN = NODE_ADDED, MAX = NODE_REMOVED };

inline const ConfigNodeEvents (&EnumValuesConfigNodeEvents())[2] {
	static const ConfigNodeEvents values[] = {ConfigNodeEvents::NODE_ADDED, ConfigNodeEvents::NODE_REMOVED};
	return values;
}

inline const char *const *EnumNamesConfigNodeEvents() {
	static const char *const names[] = {"NODE_ADDED", "NODE_REMOVED", nullptr};
	return names;
}

inline const char *EnumNameConfigNodeEvents(ConfigNodeEvents e) {
	if (e < ConfigNodeEvents::NODE_ADDED || e > ConfigNodeEvents::NODE_REMOVED)
		return "";
	const size_t index = static_cast<int>(e);
	return EnumNamesConfigNodeEvents()[index];
}

enum class ConfigAttributeEvents : int8_t {
	ATTRIBUTE_ADDED           = 0,
	ATTRIBUTE_MODIFIED        = 1,
	ATTRIBUTE_REMOVED         = 2,
	ATTRIBUTE_MODIFIED_CREATE = 3,
	MIN                       = ATTRIBUTE_ADDED,
	MAX                       = ATTRIBUTE_MODIFIED_CREATE
};

inline const ConfigAttributeEvents (&EnumValuesConfigAttributeEvents())[4] {
	static const ConfigAttributeEvents values[]
		= {ConfigAttributeEvents::ATTRIBUTE_ADDED, ConfigAttributeEvents::ATTRIBUTE_MODIFIED,
			ConfigAttributeEvents::ATTRIBUTE_REMOVED, ConfigAttributeEvents::ATTRIBUTE_MODIFIED_CREATE};
	return values;
}

inline const char *const *EnumNamesConfigAttributeEvents() {
	static const char *const names[]
		= {"ATTRIBUTE_ADDED", "ATTRIBUTE_MODIFIED", "ATTRIBUTE_REMOVED", "ATTRIBUTE_MODIFIED_CREATE", nullptr};
	return names;
}

inline const char *EnumNameConfigAttributeEvents(ConfigAttributeEvents e) {
	if (e < ConfigAttributeEvents::ATTRIBUTE_ADDED || e > ConfigAttributeEvents::ATTRIBUTE_MODIFIED_CREATE)
		return "";
	const size_t index = static_cast<int>(e);
	return EnumNamesConfigAttributeEvents()[index];
}

struct ConfigActionDataT : public flatbuffers::NativeTable {
	typedef ConfigActionData TableType;
	static FLATBUFFERS_CONSTEXPR const char *GetFullyQualifiedName() {
		return "dv.ConfigActionDataT";
	}
	ConfigAction action;
	ConfigNodeEvents nodeEvents;
	ConfigAttributeEvents attrEvents;
	uint64_t id;
	dv::cstring node;
	dv::cstring key;
	ConfigType type;
	dv::cstring value;
	dv::cstring ranges;
	int32_t flags;
	dv::cstring description;
	ConfigActionDataT() :
		action(ConfigAction::ERROR),
		nodeEvents(ConfigNodeEvents::NODE_ADDED),
		attrEvents(ConfigAttributeEvents::ATTRIBUTE_ADDED),
		id(0),
		type(ConfigType::UNKNOWN),
		flags(0) {
	}
};

inline bool operator==(const ConfigActionDataT &lhs, const ConfigActionDataT &rhs) {
	return (lhs.action == rhs.action) && (lhs.nodeEvents == rhs.nodeEvents) && (lhs.attrEvents == rhs.attrEvents)
		   && (lhs.id == rhs.id) && (lhs.node == rhs.node) && (lhs.key == rhs.key) && (lhs.type == rhs.type)
		   && (lhs.value == rhs.value) && (lhs.ranges == rhs.ranges) && (lhs.flags == rhs.flags)
		   && (lhs.description == rhs.description);
}

struct ConfigActionData FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef ConfigActionDataT NativeTableType;
	static FLATBUFFERS_CONSTEXPR const char *identifier = "CFGA";
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return ConfigActionDataTypeTable();
	}
	static FLATBUFFERS_CONSTEXPR const char *GetFullyQualifiedName() {
		return "dv.ConfigActionData";
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
		VT_ACTION      = 4,
		VT_NODEEVENTS  = 6,
		VT_ATTREVENTS  = 8,
		VT_ID          = 10,
		VT_NODE        = 12,
		VT_KEY         = 14,
		VT_TYPE        = 16,
		VT_VALUE       = 18,
		VT_RANGES      = 20,
		VT_FLAGS       = 22,
		VT_DESCRIPTION = 24
	};
	ConfigAction action() const {
		return static_cast<ConfigAction>(GetField<int8_t>(VT_ACTION, 0));
	}
	ConfigNodeEvents nodeEvents() const {
		return static_cast<ConfigNodeEvents>(GetField<int8_t>(VT_NODEEVENTS, 0));
	}
	ConfigAttributeEvents attrEvents() const {
		return static_cast<ConfigAttributeEvents>(GetField<int8_t>(VT_ATTREVENTS, 0));
	}
	uint64_t id() const {
		return GetField<uint64_t>(VT_ID, 0);
	}
	const flatbuffers::String *node() const {
		return GetPointer<const flatbuffers::String *>(VT_NODE);
	}
	const flatbuffers::String *key() const {
		return GetPointer<const flatbuffers::String *>(VT_KEY);
	}
	ConfigType type() const {
		return static_cast<ConfigType>(GetField<int8_t>(VT_TYPE, -1));
	}
	const flatbuffers::String *value() const {
		return GetPointer<const flatbuffers::String *>(VT_VALUE);
	}
	const flatbuffers::String *ranges() const {
		return GetPointer<const flatbuffers::String *>(VT_RANGES);
	}
	int32_t flags() const {
		return GetField<int32_t>(VT_FLAGS, 0);
	}
	const flatbuffers::String *description() const {
		return GetPointer<const flatbuffers::String *>(VT_DESCRIPTION);
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyField<int8_t>(verifier, VT_ACTION)
			   && VerifyField<int8_t>(verifier, VT_NODEEVENTS) && VerifyField<int8_t>(verifier, VT_ATTREVENTS)
			   && VerifyField<uint64_t>(verifier, VT_ID) && VerifyOffset(verifier, VT_NODE)
			   && verifier.VerifyString(node()) && VerifyOffset(verifier, VT_KEY) && verifier.VerifyString(key())
			   && VerifyField<int8_t>(verifier, VT_TYPE) && VerifyOffset(verifier, VT_VALUE)
			   && verifier.VerifyString(value()) && VerifyOffset(verifier, VT_RANGES) && verifier.VerifyString(ranges())
			   && VerifyField<int32_t>(verifier, VT_FLAGS) && VerifyOffset(verifier, VT_DESCRIPTION)
			   && verifier.VerifyString(description()) && verifier.EndTable();
	}
	ConfigActionDataT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(ConfigActionDataT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static void UnPackToFrom(ConfigActionDataT *_o, const ConfigActionData *_fb,
		const flatbuffers::resolver_function_t *_resolver = nullptr);
	static flatbuffers::Offset<ConfigActionData> Pack(flatbuffers::FlatBufferBuilder &_fbb, const ConfigActionDataT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct ConfigActionDataBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_action(ConfigAction action) {
		fbb_.AddElement<int8_t>(ConfigActionData::VT_ACTION, static_cast<int8_t>(action), 0);
	}
	void add_nodeEvents(ConfigNodeEvents nodeEvents) {
		fbb_.AddElement<int8_t>(ConfigActionData::VT_NODEEVENTS, static_cast<int8_t>(nodeEvents), 0);
	}
	void add_attrEvents(ConfigAttributeEvents attrEvents) {
		fbb_.AddElement<int8_t>(ConfigActionData::VT_ATTREVENTS, static_cast<int8_t>(attrEvents), 0);
	}
	void add_id(uint64_t id) {
		fbb_.AddElement<uint64_t>(ConfigActionData::VT_ID, id, 0);
	}
	void add_node(flatbuffers::Offset<flatbuffers::String> node) {
		fbb_.AddOffset(ConfigActionData::VT_NODE, node);
	}
	void add_key(flatbuffers::Offset<flatbuffers::String> key) {
		fbb_.AddOffset(ConfigActionData::VT_KEY, key);
	}
	void add_type(ConfigType type) {
		fbb_.AddElement<int8_t>(ConfigActionData::VT_TYPE, static_cast<int8_t>(type), -1);
	}
	void add_value(flatbuffers::Offset<flatbuffers::String> value) {
		fbb_.AddOffset(ConfigActionData::VT_VALUE, value);
	}
	void add_ranges(flatbuffers::Offset<flatbuffers::String> ranges) {
		fbb_.AddOffset(ConfigActionData::VT_RANGES, ranges);
	}
	void add_flags(int32_t flags) {
		fbb_.AddElement<int32_t>(ConfigActionData::VT_FLAGS, flags, 0);
	}
	void add_description(flatbuffers::Offset<flatbuffers::String> description) {
		fbb_.AddOffset(ConfigActionData::VT_DESCRIPTION, description);
	}
	explicit ConfigActionDataBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	ConfigActionDataBuilder &operator=(const ConfigActionDataBuilder &);
	flatbuffers::Offset<ConfigActionData> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<ConfigActionData>(end);
		return o;
	}
};

inline flatbuffers::Offset<ConfigActionData> CreateConfigActionData(flatbuffers::FlatBufferBuilder &_fbb,
	ConfigAction action = ConfigAction::ERROR, ConfigNodeEvents nodeEvents = ConfigNodeEvents::NODE_ADDED,
	ConfigAttributeEvents attrEvents = ConfigAttributeEvents::ATTRIBUTE_ADDED, uint64_t id = 0,
	flatbuffers::Offset<flatbuffers::String> node = 0, flatbuffers::Offset<flatbuffers::String> key = 0,
	ConfigType type = ConfigType::UNKNOWN, flatbuffers::Offset<flatbuffers::String> value = 0,
	flatbuffers::Offset<flatbuffers::String> ranges = 0, int32_t flags = 0,
	flatbuffers::Offset<flatbuffers::String> description = 0) {
	ConfigActionDataBuilder builder_(_fbb);
	builder_.add_id(id);
	builder_.add_description(description);
	builder_.add_flags(flags);
	builder_.add_ranges(ranges);
	builder_.add_value(value);
	builder_.add_key(key);
	builder_.add_node(node);
	builder_.add_type(type);
	builder_.add_attrEvents(attrEvents);
	builder_.add_nodeEvents(nodeEvents);
	builder_.add_action(action);
	return builder_.Finish();
}

inline flatbuffers::Offset<ConfigActionData> CreateConfigActionDataDirect(flatbuffers::FlatBufferBuilder &_fbb,
	ConfigAction action = ConfigAction::ERROR, ConfigNodeEvents nodeEvents = ConfigNodeEvents::NODE_ADDED,
	ConfigAttributeEvents attrEvents = ConfigAttributeEvents::ATTRIBUTE_ADDED, uint64_t id = 0,
	const char *node = nullptr, const char *key = nullptr, ConfigType type = ConfigType::UNKNOWN,
	const char *value = nullptr, const char *ranges = nullptr, int32_t flags = 0, const char *description = nullptr) {
	auto node__        = node ? _fbb.CreateString(node) : 0;
	auto key__         = key ? _fbb.CreateString(key) : 0;
	auto value__       = value ? _fbb.CreateString(value) : 0;
	auto ranges__      = ranges ? _fbb.CreateString(ranges) : 0;
	auto description__ = description ? _fbb.CreateString(description) : 0;
	return dv::CreateConfigActionData(
		_fbb, action, nodeEvents, attrEvents, id, node__, key__, type, value__, ranges__, flags, description__);
}

flatbuffers::Offset<ConfigActionData> CreateConfigActionData(flatbuffers::FlatBufferBuilder &_fbb,
	const ConfigActionDataT *_o, const flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline ConfigActionDataT *ConfigActionData::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new ConfigActionDataT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void ConfigActionData::UnPackTo(ConfigActionDataT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	UnPackToFrom(_o, this, _resolver);
}

inline void ConfigActionData::UnPackToFrom(
	ConfigActionDataT *_o, const ConfigActionData *_fb, const flatbuffers::resolver_function_t *_resolver) {
	(void) _o;
	(void) _fb;
	(void) _resolver;
	{
		auto _e    = _fb->action();
		_o->action = _e;
	};
	{
		auto _e        = _fb->nodeEvents();
		_o->nodeEvents = _e;
	};
	{
		auto _e        = _fb->attrEvents();
		_o->attrEvents = _e;
	};
	{
		auto _e = _fb->id();
		_o->id  = _e;
	};
	{
		auto _e = _fb->node();
		if (_e)
			_o->node = dv::cstring(_e->c_str(), _e->size());
	};
	{
		auto _e = _fb->key();
		if (_e)
			_o->key = dv::cstring(_e->c_str(), _e->size());
	};
	{
		auto _e  = _fb->type();
		_o->type = _e;
	};
	{
		auto _e = _fb->value();
		if (_e)
			_o->value = dv::cstring(_e->c_str(), _e->size());
	};
	{
		auto _e = _fb->ranges();
		if (_e)
			_o->ranges = dv::cstring(_e->c_str(), _e->size());
	};
	{
		auto _e   = _fb->flags();
		_o->flags = _e;
	};
	{
		auto _e = _fb->description();
		if (_e)
			_o->description = dv::cstring(_e->c_str(), _e->size());
	};
}

inline flatbuffers::Offset<ConfigActionData> ConfigActionData::Pack(flatbuffers::FlatBufferBuilder &_fbb,
	const ConfigActionDataT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateConfigActionData(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<ConfigActionData> CreateConfigActionData(flatbuffers::FlatBufferBuilder &_fbb,
	const ConfigActionDataT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const ConfigActionDataT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _action      = _o->action;
	auto _nodeEvents  = _o->nodeEvents;
	auto _attrEvents  = _o->attrEvents;
	auto _id          = _o->id;
	auto _node        = _o->node.empty() ? 0 : _fbb.CreateString(_o->node);
	auto _key         = _o->key.empty() ? 0 : _fbb.CreateString(_o->key);
	auto _type        = _o->type;
	auto _value       = _o->value.empty() ? 0 : _fbb.CreateString(_o->value);
	auto _ranges      = _o->ranges.empty() ? 0 : _fbb.CreateString(_o->ranges);
	auto _flags       = _o->flags;
	auto _description = _o->description.empty() ? 0 : _fbb.CreateString(_o->description);
	return dv::CreateConfigActionData(
		_fbb, _action, _nodeEvents, _attrEvents, _id, _node, _key, _type, _value, _ranges, _flags, _description);
}

inline const flatbuffers::TypeTable *ConfigActionTypeTable() {
	static const flatbuffers::TypeCode type_codes[]
		= {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {ConfigActionTypeTable};
	static const char *const names[]       = {"ERROR", "NODE_EXISTS", "ATTR_EXISTS", "GET_CHILDREN", "GET_ATTRIBUTES",
        "GET_TYPE", "GET_RANGES", "GET_FLAGS", "GET_DESCRIPTION", "GET", "PUT", "ADD_MODULE", "REMOVE_MODULE",
        "ADD_PUSH_CLIENT", "REMOVE_PUSH_CLIENT", "PUSH_MESSAGE_NODE", "PUSH_MESSAGE_ATTR", "DUMP_TREE",
        "DUMP_TREE_NODE", "DUMP_TREE_ATTR", "GET_CLIENT_ID"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 21, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *ConfigTypeTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
        {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
        {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {ConfigTypeTypeTable};
	static const int64_t values[]                      = {-1, 0, 1, 2, 3, 4, 5};
	static const char *const names[]       = {"UNKNOWN", "BOOL", "INT", "LONG", "FLOAT", "DOUBLE", "STRING"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 7, type_codes, type_refs, values, names};
	return &tt;
}

inline const flatbuffers::TypeTable *ConfigNodeEventsTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {ConfigNodeEventsTypeTable};
	static const char *const names[]                   = {"NODE_ADDED", "NODE_REMOVED"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 2, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *ConfigAttributeEventsTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
        {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {ConfigAttributeEventsTypeTable};
	static const char *const names[]
		= {"ATTRIBUTE_ADDED", "ATTRIBUTE_MODIFIED", "ATTRIBUTE_REMOVED", "ATTRIBUTE_MODIFIED_CREATE"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 4, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *ConfigActionDataTypeTable() {
	static const flatbuffers::TypeCode type_codes[] = {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 1},
		{flatbuffers::ET_CHAR, 0, 2}, {flatbuffers::ET_ULONG, 0, -1}, {flatbuffers::ET_STRING, 0, -1},
		{flatbuffers::ET_STRING, 0, -1}, {flatbuffers::ET_CHAR, 0, 3}, {flatbuffers::ET_STRING, 0, -1},
		{flatbuffers::ET_STRING, 0, -1}, {flatbuffers::ET_INT, 0, -1}, {flatbuffers::ET_STRING, 0, -1}};
	static const flatbuffers::TypeFunction type_refs[]
		= {ConfigActionTypeTable, ConfigNodeEventsTypeTable, ConfigAttributeEventsTypeTable, ConfigTypeTypeTable};
	static const char *const names[] = {
		"action", "nodeEvents", "attrEvents", "id", "node", "key", "type", "value", "ranges", "flags", "description"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 11, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const dv::ConfigActionData *GetConfigActionData(const void *buf) {
	return flatbuffers::GetRoot<dv::ConfigActionData>(buf);
}

inline const dv::ConfigActionData *GetSizePrefixedConfigActionData(const void *buf) {
	return flatbuffers::GetSizePrefixedRoot<dv::ConfigActionData>(buf);
}

inline FLATBUFFERS_CONSTEXPR const char *ConfigActionDataIdentifier() {
	return dv::ConfigActionData::identifier;
}

inline bool ConfigActionDataBufferHasIdentifier(const void *buf) {
	return flatbuffers::BufferHasIdentifier(buf, ConfigActionDataIdentifier());
}

inline bool VerifyConfigActionDataBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifyBuffer<dv::ConfigActionData>(ConfigActionDataIdentifier());
}

inline bool VerifySizePrefixedConfigActionDataBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifySizePrefixedBuffer<dv::ConfigActionData>(ConfigActionDataIdentifier());
}

inline void FinishConfigActionDataBuffer(
	flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<dv::ConfigActionData> root) {
	fbb.Finish(root, ConfigActionDataIdentifier());
}

inline void FinishSizePrefixedConfigActionDataBuffer(
	flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<dv::ConfigActionData> root) {
	fbb.FinishSizePrefixed(root, ConfigActionDataIdentifier());
}

inline std::unique_ptr<ConfigActionDataT> UnPackConfigActionData(
	const void *buf, const flatbuffers::resolver_function_t *res = nullptr) {
	return std::unique_ptr<ConfigActionDataT>(GetConfigActionData(buf)->UnPack(res));
}

} // namespace dv

#endif // FLATBUFFERS_GENERATED_DVCONFIGACTIONDATA_DV_H_
