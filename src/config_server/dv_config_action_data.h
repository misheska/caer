// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_DVCONFIGACTIONDATA_DV_H_
#define FLATBUFFERS_GENERATED_DVCONFIGACTIONDATA_DV_H_

#include "flatbuffers/flatbuffers.h"

namespace dv {

struct ConfigActionData;
struct ConfigActionDataT;

enum class ConfigAction : uint8_t {
  NODE_EXISTS = 0,
  ATTR_EXISTS = 1,
  GET = 2,
  PUT = 3,
  ERROR = 4,
  GET_CHILDREN = 5,
  GET_ATTRIBUTES = 6,
  GET_TYPE = 7,
  GET_RANGES = 8,
  GET_FLAGS = 9,
  GET_DESCRIPTION = 10,
  ADD_MODULE = 11,
  REMOVE_MODULE = 12,
  ADD_PUSH_CLIENT = 13,
  REMOVE_PUSH_CLIENT = 14,
  PUSH_MESSAGE_NODE = 15,
  PUSH_MESSAGE_ATTR = 16,
  DUMP_TREE = 17,
  DUMP_TREE_NODE = 18,
  DUMP_TREE_ATTR = 19,
  MIN = NODE_EXISTS,
  MAX = DUMP_TREE_ATTR
};

inline const ConfigAction (&EnumValuesConfigAction())[20] {
  static const ConfigAction values[] = {
    ConfigAction::NODE_EXISTS,
    ConfigAction::ATTR_EXISTS,
    ConfigAction::GET,
    ConfigAction::PUT,
    ConfigAction::ERROR,
    ConfigAction::GET_CHILDREN,
    ConfigAction::GET_ATTRIBUTES,
    ConfigAction::GET_TYPE,
    ConfigAction::GET_RANGES,
    ConfigAction::GET_FLAGS,
    ConfigAction::GET_DESCRIPTION,
    ConfigAction::ADD_MODULE,
    ConfigAction::REMOVE_MODULE,
    ConfigAction::ADD_PUSH_CLIENT,
    ConfigAction::REMOVE_PUSH_CLIENT,
    ConfigAction::PUSH_MESSAGE_NODE,
    ConfigAction::PUSH_MESSAGE_ATTR,
    ConfigAction::DUMP_TREE,
    ConfigAction::DUMP_TREE_NODE,
    ConfigAction::DUMP_TREE_ATTR
  };
  return values;
}

inline const char * const *EnumNamesConfigAction() {
  static const char * const names[] = {
    "NODE_EXISTS",
    "ATTR_EXISTS",
    "GET",
    "PUT",
    "ERROR",
    "GET_CHILDREN",
    "GET_ATTRIBUTES",
    "GET_TYPE",
    "GET_RANGES",
    "GET_FLAGS",
    "GET_DESCRIPTION",
    "ADD_MODULE",
    "REMOVE_MODULE",
    "ADD_PUSH_CLIENT",
    "REMOVE_PUSH_CLIENT",
    "PUSH_MESSAGE_NODE",
    "PUSH_MESSAGE_ATTR",
    "DUMP_TREE",
    "DUMP_TREE_NODE",
    "DUMP_TREE_ATTR",
    nullptr
  };
  return names;
}

inline const char *EnumNameConfigAction(ConfigAction e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesConfigAction()[index];
}

enum class ConfigType : int8_t {
  UNKNOWN = -1,
  BOOL = 0,
  INT = 3,
  LONG = 4,
  FLOAT = 5,
  DOUBLE = 6,
  STRING = 7,
  MIN = UNKNOWN,
  MAX = STRING
};

inline const ConfigType (&EnumValuesConfigType())[7] {
  static const ConfigType values[] = {
    ConfigType::UNKNOWN,
    ConfigType::BOOL,
    ConfigType::INT,
    ConfigType::LONG,
    ConfigType::FLOAT,
    ConfigType::DOUBLE,
    ConfigType::STRING
  };
  return values;
}

inline const char * const *EnumNamesConfigType() {
  static const char * const names[] = {
    "UNKNOWN",
    "BOOL",
    "",
    "",
    "INT",
    "LONG",
    "FLOAT",
    "DOUBLE",
    "STRING",
    nullptr
  };
  return names;
}

inline const char *EnumNameConfigType(ConfigType e) {
  const size_t index = static_cast<int>(e) - static_cast<int>(ConfigType::UNKNOWN);
  return EnumNamesConfigType()[index];
}

enum class ConfigNodeEvents : uint8_t {
  DVCFG_NODE_CHILD_ADDED = 0,
  DVCFG_NODE_CHILD_REMOVED = 1,
  MIN = DVCFG_NODE_CHILD_ADDED,
  MAX = DVCFG_NODE_CHILD_REMOVED
};

inline const ConfigNodeEvents (&EnumValuesConfigNodeEvents())[2] {
  static const ConfigNodeEvents values[] = {
    ConfigNodeEvents::DVCFG_NODE_CHILD_ADDED,
    ConfigNodeEvents::DVCFG_NODE_CHILD_REMOVED
  };
  return values;
}

inline const char * const *EnumNamesConfigNodeEvents() {
  static const char * const names[] = {
    "DVCFG_NODE_CHILD_ADDED",
    "DVCFG_NODE_CHILD_REMOVED",
    nullptr
  };
  return names;
}

inline const char *EnumNameConfigNodeEvents(ConfigNodeEvents e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesConfigNodeEvents()[index];
}

enum class ConfigAttributeEvents : uint8_t {
  DVCFG_ATTRIBUTE_ADDED = 0,
  DVCFG_ATTRIBUTE_MODIFIED = 1,
  DVCFG_ATTRIBUTE_REMOVED = 2,
  MIN = DVCFG_ATTRIBUTE_ADDED,
  MAX = DVCFG_ATTRIBUTE_REMOVED
};

inline const ConfigAttributeEvents (&EnumValuesConfigAttributeEvents())[3] {
  static const ConfigAttributeEvents values[] = {
    ConfigAttributeEvents::DVCFG_ATTRIBUTE_ADDED,
    ConfigAttributeEvents::DVCFG_ATTRIBUTE_MODIFIED,
    ConfigAttributeEvents::DVCFG_ATTRIBUTE_REMOVED
  };
  return values;
}

inline const char * const *EnumNamesConfigAttributeEvents() {
  static const char * const names[] = {
    "DVCFG_ATTRIBUTE_ADDED",
    "DVCFG_ATTRIBUTE_MODIFIED",
    "DVCFG_ATTRIBUTE_REMOVED",
    nullptr
  };
  return names;
}

inline const char *EnumNameConfigAttributeEvents(ConfigAttributeEvents e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesConfigAttributeEvents()[index];
}

struct ConfigActionDataT : public flatbuffers::NativeTable {
  typedef ConfigActionData TableType;
  ConfigAction action;
  ConfigNodeEvents nodeEvents;
  ConfigAttributeEvents attrEvents;
  uint64_t id;
  std::string node;
  std::string key;
  ConfigType type;
  std::string value;
  std::string ranges;
  int32_t flags;
  std::string description;
  ConfigActionDataT()
      : action(ConfigAction::NODE_EXISTS),
        nodeEvents(ConfigNodeEvents::DVCFG_NODE_CHILD_ADDED),
        attrEvents(ConfigAttributeEvents::DVCFG_ATTRIBUTE_ADDED),
        id(0),
        type(ConfigType::BOOL),
        flags(0) {
  }
};

struct ConfigActionData FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef ConfigActionDataT NativeTableType;
  enum {
    VT_ACTION = 4,
    VT_NODEEVENTS = 6,
    VT_ATTREVENTS = 8,
    VT_ID = 10,
    VT_NODE = 12,
    VT_KEY = 14,
    VT_TYPE = 16,
    VT_VALUE = 18,
    VT_RANGES = 20,
    VT_FLAGS = 22,
    VT_DESCRIPTION = 24
  };
  ConfigAction action() const {
    return static_cast<ConfigAction>(GetField<uint8_t>(VT_ACTION, 0));
  }
  ConfigNodeEvents nodeEvents() const {
    return static_cast<ConfigNodeEvents>(GetField<uint8_t>(VT_NODEEVENTS, 0));
  }
  ConfigAttributeEvents attrEvents() const {
    return static_cast<ConfigAttributeEvents>(GetField<uint8_t>(VT_ATTREVENTS, 0));
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
    return static_cast<ConfigType>(GetField<int8_t>(VT_TYPE, 0));
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
    return VerifyTableStart(verifier) &&
           VerifyField<uint8_t>(verifier, VT_ACTION) &&
           VerifyField<uint8_t>(verifier, VT_NODEEVENTS) &&
           VerifyField<uint8_t>(verifier, VT_ATTREVENTS) &&
           VerifyField<uint64_t>(verifier, VT_ID) &&
           VerifyOffset(verifier, VT_NODE) &&
           verifier.VerifyString(node()) &&
           VerifyOffset(verifier, VT_KEY) &&
           verifier.VerifyString(key()) &&
           VerifyField<int8_t>(verifier, VT_TYPE) &&
           VerifyOffset(verifier, VT_VALUE) &&
           verifier.VerifyString(value()) &&
           VerifyOffset(verifier, VT_RANGES) &&
           verifier.VerifyString(ranges()) &&
           VerifyField<int32_t>(verifier, VT_FLAGS) &&
           VerifyOffset(verifier, VT_DESCRIPTION) &&
           verifier.VerifyString(description()) &&
           verifier.EndTable();
  }
  ConfigActionDataT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
  void UnPackTo(ConfigActionDataT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
  static flatbuffers::Offset<ConfigActionData> Pack(flatbuffers::FlatBufferBuilder &_fbb, const ConfigActionDataT* _o, const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct ConfigActionDataBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_action(ConfigAction action) {
    fbb_.AddElement<uint8_t>(ConfigActionData::VT_ACTION, static_cast<uint8_t>(action), 0);
  }
  void add_nodeEvents(ConfigNodeEvents nodeEvents) {
    fbb_.AddElement<uint8_t>(ConfigActionData::VT_NODEEVENTS, static_cast<uint8_t>(nodeEvents), 0);
  }
  void add_attrEvents(ConfigAttributeEvents attrEvents) {
    fbb_.AddElement<uint8_t>(ConfigActionData::VT_ATTREVENTS, static_cast<uint8_t>(attrEvents), 0);
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
    fbb_.AddElement<int8_t>(ConfigActionData::VT_TYPE, static_cast<int8_t>(type), 0);
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
  explicit ConfigActionDataBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ConfigActionDataBuilder &operator=(const ConfigActionDataBuilder &);
  flatbuffers::Offset<ConfigActionData> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<ConfigActionData>(end);
    return o;
  }
};

inline flatbuffers::Offset<ConfigActionData> CreateConfigActionData(
    flatbuffers::FlatBufferBuilder &_fbb,
    ConfigAction action = ConfigAction::NODE_EXISTS,
    ConfigNodeEvents nodeEvents = ConfigNodeEvents::DVCFG_NODE_CHILD_ADDED,
    ConfigAttributeEvents attrEvents = ConfigAttributeEvents::DVCFG_ATTRIBUTE_ADDED,
    uint64_t id = 0,
    flatbuffers::Offset<flatbuffers::String> node = 0,
    flatbuffers::Offset<flatbuffers::String> key = 0,
    ConfigType type = ConfigType::BOOL,
    flatbuffers::Offset<flatbuffers::String> value = 0,
    flatbuffers::Offset<flatbuffers::String> ranges = 0,
    int32_t flags = 0,
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

inline flatbuffers::Offset<ConfigActionData> CreateConfigActionDataDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    ConfigAction action = ConfigAction::NODE_EXISTS,
    ConfigNodeEvents nodeEvents = ConfigNodeEvents::DVCFG_NODE_CHILD_ADDED,
    ConfigAttributeEvents attrEvents = ConfigAttributeEvents::DVCFG_ATTRIBUTE_ADDED,
    uint64_t id = 0,
    const char *node = nullptr,
    const char *key = nullptr,
    ConfigType type = ConfigType::BOOL,
    const char *value = nullptr,
    const char *ranges = nullptr,
    int32_t flags = 0,
    const char *description = nullptr) {
  return dv::CreateConfigActionData(
      _fbb,
      action,
      nodeEvents,
      attrEvents,
      id,
      node ? _fbb.CreateString(node) : 0,
      key ? _fbb.CreateString(key) : 0,
      type,
      value ? _fbb.CreateString(value) : 0,
      ranges ? _fbb.CreateString(ranges) : 0,
      flags,
      description ? _fbb.CreateString(description) : 0);
}

flatbuffers::Offset<ConfigActionData> CreateConfigActionData(flatbuffers::FlatBufferBuilder &_fbb, const ConfigActionDataT *_o, const flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline ConfigActionDataT *ConfigActionData::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
  auto _o = new ConfigActionDataT();
  UnPackTo(_o, _resolver);
  return _o;
}

inline void ConfigActionData::UnPackTo(ConfigActionDataT *_o, const flatbuffers::resolver_function_t *_resolver) const {
  (void)_o;
  (void)_resolver;
  { auto _e = action(); _o->action = _e; };
  { auto _e = nodeEvents(); _o->nodeEvents = _e; };
  { auto _e = attrEvents(); _o->attrEvents = _e; };
  { auto _e = id(); _o->id = _e; };
  { auto _e = node(); if (_e) _o->node = _e->str(); };
  { auto _e = key(); if (_e) _o->key = _e->str(); };
  { auto _e = type(); _o->type = _e; };
  { auto _e = value(); if (_e) _o->value = _e->str(); };
  { auto _e = ranges(); if (_e) _o->ranges = _e->str(); };
  { auto _e = flags(); _o->flags = _e; };
  { auto _e = description(); if (_e) _o->description = _e->str(); };
}

inline flatbuffers::Offset<ConfigActionData> ConfigActionData::Pack(flatbuffers::FlatBufferBuilder &_fbb, const ConfigActionDataT* _o, const flatbuffers::rehasher_function_t *_rehasher) {
  return CreateConfigActionData(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<ConfigActionData> CreateConfigActionData(flatbuffers::FlatBufferBuilder &_fbb, const ConfigActionDataT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
  (void)_rehasher;
  (void)_o;
  struct _VectorArgs { flatbuffers::FlatBufferBuilder *__fbb; const ConfigActionDataT* __o; const flatbuffers::rehasher_function_t *__rehasher; } _va = { &_fbb, _o, _rehasher}; (void)_va;
  auto _action = _o->action;
  auto _nodeEvents = _o->nodeEvents;
  auto _attrEvents = _o->attrEvents;
  auto _id = _o->id;
  auto _node = _o->node.empty() ? 0 : _fbb.CreateString(_o->node);
  auto _key = _o->key.empty() ? 0 : _fbb.CreateString(_o->key);
  auto _type = _o->type;
  auto _value = _o->value.empty() ? 0 : _fbb.CreateString(_o->value);
  auto _ranges = _o->ranges.empty() ? 0 : _fbb.CreateString(_o->ranges);
  auto _flags = _o->flags;
  auto _description = _o->description.empty() ? 0 : _fbb.CreateString(_o->description);
  return dv::CreateConfigActionData(
      _fbb,
      _action,
      _nodeEvents,
      _attrEvents,
      _id,
      _node,
      _key,
      _type,
      _value,
      _ranges,
      _flags,
      _description);
}

inline const dv::ConfigActionData *GetConfigActionData(const void *buf) {
  return flatbuffers::GetRoot<dv::ConfigActionData>(buf);
}

inline const dv::ConfigActionData *GetSizePrefixedConfigActionData(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<dv::ConfigActionData>(buf);
}

inline bool VerifyConfigActionDataBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<dv::ConfigActionData>(nullptr);
}

inline bool VerifySizePrefixedConfigActionDataBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<dv::ConfigActionData>(nullptr);
}

inline void FinishConfigActionDataBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<dv::ConfigActionData> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedConfigActionDataBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<dv::ConfigActionData> root) {
  fbb.FinishSizePrefixed(root);
}

inline std::unique_ptr<ConfigActionDataT> UnPackConfigActionData(
    const void *buf,
    const flatbuffers::resolver_function_t *res = nullptr) {
  return std::unique_ptr<ConfigActionDataT>(GetConfigActionData(buf)->UnPack(res));
}

}  // namespace dv

#endif  // FLATBUFFERS_GENERATED_DVCONFIGACTIONDATA_DV_H_
