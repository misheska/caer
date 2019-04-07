// automatically generated by the FlatBuffers compiler, do not modify

#ifndef FLATBUFFERS_GENERATED_TRIGGER_DV_H_
#define FLATBUFFERS_GENERATED_TRIGGER_DV_H_

#include "cvector.hpp"
#include "flatbuffers/flatbuffers.h"

namespace dv {

struct Trigger;
struct TriggerT;

struct TriggerPacket;
struct TriggerPacketT;

bool operator==(const TriggerT &lhs, const TriggerT &rhs);
bool operator==(const TriggerPacketT &lhs, const TriggerPacketT &rhs);

inline const flatbuffers::TypeTable *TriggerTypeTable();

inline const flatbuffers::TypeTable *TriggerPacketTypeTable();

enum class TriggerType : int8_t {
	/// A timestamp reset occurred.
	TIMESTAMP_RESET = 0 /// A rising edge was detected (External Input module on device).
	,
	EXTERNAL_SIGNAL_RISING_EDGE = 1 /// A falling edge was detected (External Input module on device).
	,
	EXTERNAL_SIGNAL_FALLING_EDGE = 2 /// A pulse was detected (External Input module on device).
	,
	EXTERNAL_SIGNAL_PULSE = 3 /// A rising edge was generated (External Generator module on device).
	,
	EXTERNAL_GENERATOR_RISING_EDGE = 4 /// A falling edge was generated (External Generator module on device).
	,
	EXTERNAL_GENERATOR_FALLING_EDGE = 5 /// An APS frame capture has started (Frame Event will follow).
	,
	APS_FRAME_START = 6 /// An APS frame capture has completed (Frame Event is contemporary).
	,
	APS_FRAME_END = 7 /// An APS frame exposure has started (Frame Event will follow).
	,
	APS_EXPOSURE_START = 8 /// An APS frame exposure has completed (Frame Event will follow).
	,
	APS_EXPOSURE_END = 9,
	MIN              = TIMESTAMP_RESET,
	MAX              = APS_EXPOSURE_END
};

inline const TriggerType (&EnumValuesTriggerType())[10] {
	static const TriggerType values[] = {TriggerType::TIMESTAMP_RESET, TriggerType::EXTERNAL_SIGNAL_RISING_EDGE,
		TriggerType::EXTERNAL_SIGNAL_FALLING_EDGE, TriggerType::EXTERNAL_SIGNAL_PULSE,
		TriggerType::EXTERNAL_GENERATOR_RISING_EDGE, TriggerType::EXTERNAL_GENERATOR_FALLING_EDGE,
		TriggerType::APS_FRAME_START, TriggerType::APS_FRAME_END, TriggerType::APS_EXPOSURE_START,
		TriggerType::APS_EXPOSURE_END};
	return values;
}

inline const char *const *EnumNamesTriggerType() {
	static const char *const names[]
		= {"TIMESTAMP_RESET", "EXTERNAL_SIGNAL_RISING_EDGE", "EXTERNAL_SIGNAL_FALLING_EDGE", "EXTERNAL_SIGNAL_PULSE",
			"EXTERNAL_GENERATOR_RISING_EDGE", "EXTERNAL_GENERATOR_FALLING_EDGE", "APS_FRAME_START", "APS_FRAME_END",
			"APS_EXPOSURE_START", "APS_EXPOSURE_END", nullptr};
	return names;
}

inline const char *EnumNameTriggerType(TriggerType e) {
	if (e < TriggerType::TIMESTAMP_RESET || e > TriggerType::APS_EXPOSURE_END)
		return "";
	const size_t index = static_cast<int>(e);
	return EnumNamesTriggerType()[index];
}

struct TriggerT : public flatbuffers::NativeTable {
	typedef Trigger TableType;
	static FLATBUFFERS_CONSTEXPR const char *GetFullyQualifiedName() {
		return "dv.TriggerT";
	}
	int64_t timestamp;
	TriggerType type;
	TriggerT() : timestamp(0), type(TriggerType::TIMESTAMP_RESET) {
	}
};

inline bool operator==(const TriggerT &lhs, const TriggerT &rhs) {
	return (lhs.timestamp == rhs.timestamp) && (lhs.type == rhs.type);
}

struct Trigger FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef TriggerT NativeTableType;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return TriggerTypeTable();
	}
	static FLATBUFFERS_CONSTEXPR const char *GetFullyQualifiedName() {
		return "dv.Trigger";
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE { VT_TIMESTAMP = 4, VT_TYPE = 6 };
	/// Timestamp (µs).
	int64_t timestamp() const {
		return GetField<int64_t>(VT_TIMESTAMP, 0);
	}
	/// Type of trigger that occurred.
	TriggerType type() const {
		return static_cast<TriggerType>(GetField<int8_t>(VT_TYPE, 0));
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyField<int64_t>(verifier, VT_TIMESTAMP)
			   && VerifyField<int8_t>(verifier, VT_TYPE) && verifier.EndTable();
	}
	TriggerT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(TriggerT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static void UnPackToFrom(
		TriggerT *_o, const Trigger *_fb, const flatbuffers::resolver_function_t *_resolver = nullptr);
	static flatbuffers::Offset<Trigger> Pack(flatbuffers::FlatBufferBuilder &_fbb, const TriggerT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct TriggerBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_timestamp(int64_t timestamp) {
		fbb_.AddElement<int64_t>(Trigger::VT_TIMESTAMP, timestamp, 0);
	}
	void add_type(TriggerType type) {
		fbb_.AddElement<int8_t>(Trigger::VT_TYPE, static_cast<int8_t>(type), 0);
	}
	explicit TriggerBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	TriggerBuilder &operator=(const TriggerBuilder &);
	flatbuffers::Offset<Trigger> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<Trigger>(end);
		return o;
	}
};

inline flatbuffers::Offset<Trigger> CreateTrigger(
	flatbuffers::FlatBufferBuilder &_fbb, int64_t timestamp = 0, TriggerType type = TriggerType::TIMESTAMP_RESET) {
	TriggerBuilder builder_(_fbb);
	builder_.add_timestamp(timestamp);
	builder_.add_type(type);
	return builder_.Finish();
}

flatbuffers::Offset<Trigger> CreateTrigger(flatbuffers::FlatBufferBuilder &_fbb, const TriggerT *_o,
	const flatbuffers::rehasher_function_t *_rehasher = nullptr);

struct TriggerPacketT : public flatbuffers::NativeTable {
	typedef TriggerPacket TableType;
	static FLATBUFFERS_CONSTEXPR const char *GetFullyQualifiedName() {
		return "dv.TriggerPacketT";
	}
	dv::cvector<TriggerT> triggers;
	TriggerPacketT() {
	}
};

inline bool operator==(const TriggerPacketT &lhs, const TriggerPacketT &rhs) {
	return (lhs.triggers == rhs.triggers);
}

struct TriggerPacket FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef TriggerPacketT NativeTableType;
	static const char *identifier;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return TriggerPacketTypeTable();
	}
	static FLATBUFFERS_CONSTEXPR const char *GetFullyQualifiedName() {
		return "dv.TriggerPacket";
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE { VT_TRIGGERS = 4 };
	const flatbuffers::Vector<flatbuffers::Offset<Trigger>> *triggers() const {
		return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Trigger>> *>(VT_TRIGGERS);
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyOffset(verifier, VT_TRIGGERS) && verifier.VerifyVector(triggers())
			   && verifier.VerifyVectorOfTables(triggers()) && verifier.EndTable();
	}
	TriggerPacketT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(TriggerPacketT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static void UnPackToFrom(
		TriggerPacketT *_o, const TriggerPacket *_fb, const flatbuffers::resolver_function_t *_resolver = nullptr);
	static flatbuffers::Offset<TriggerPacket> Pack(flatbuffers::FlatBufferBuilder &_fbb, const TriggerPacketT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct TriggerPacketBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_triggers(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Trigger>>> triggers) {
		fbb_.AddOffset(TriggerPacket::VT_TRIGGERS, triggers);
	}
	explicit TriggerPacketBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	TriggerPacketBuilder &operator=(const TriggerPacketBuilder &);
	flatbuffers::Offset<TriggerPacket> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<TriggerPacket>(end);
		return o;
	}
};

inline flatbuffers::Offset<TriggerPacket> CreateTriggerPacket(flatbuffers::FlatBufferBuilder &_fbb,
	flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Trigger>>> triggers = 0) {
	TriggerPacketBuilder builder_(_fbb);
	builder_.add_triggers(triggers);
	return builder_.Finish();
}

inline flatbuffers::Offset<TriggerPacket> CreateTriggerPacketDirect(
	flatbuffers::FlatBufferBuilder &_fbb, const std::vector<flatbuffers::Offset<Trigger>> *triggers = nullptr) {
	auto triggers__ = triggers ? _fbb.CreateVector<flatbuffers::Offset<Trigger>>(*triggers) : 0;
	return dv::CreateTriggerPacket(_fbb, triggers__);
}

flatbuffers::Offset<TriggerPacket> CreateTriggerPacket(flatbuffers::FlatBufferBuilder &_fbb, const TriggerPacketT *_o,
	const flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline TriggerT *Trigger::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new TriggerT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void Trigger::UnPackTo(TriggerT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	UnPackToFrom(_o, this, _resolver);
}

inline void Trigger::UnPackToFrom(TriggerT *_o, const Trigger *_fb, const flatbuffers::resolver_function_t *_resolver) {
	(void) _o;
	(void) _fb;
	(void) _resolver;
	{
		auto _e       = _fb->timestamp();
		_o->timestamp = _e;
	};
	{
		auto _e  = _fb->type();
		_o->type = _e;
	};
}

inline flatbuffers::Offset<Trigger> Trigger::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const TriggerT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateTrigger(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<Trigger> CreateTrigger(
	flatbuffers::FlatBufferBuilder &_fbb, const TriggerT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const TriggerT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _timestamp = _o->timestamp;
	auto _type      = _o->type;
	return dv::CreateTrigger(_fbb, _timestamp, _type);
}

inline TriggerPacketT *TriggerPacket::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new TriggerPacketT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void TriggerPacket::UnPackTo(TriggerPacketT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	UnPackToFrom(_o, this, _resolver);
}

inline void TriggerPacket::UnPackToFrom(
	TriggerPacketT *_o, const TriggerPacket *_fb, const flatbuffers::resolver_function_t *_resolver) {
	(void) _o;
	(void) _fb;
	(void) _resolver;
	{
		auto _e = _fb->triggers();
		if (_e) {
			_o->triggers.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_e->Get(_i)->UnPackTo(&_o->triggers[_i], _resolver);
			}
		}
	};
}

inline flatbuffers::Offset<TriggerPacket> TriggerPacket::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const TriggerPacketT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateTriggerPacket(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<TriggerPacket> CreateTriggerPacket(
	flatbuffers::FlatBufferBuilder &_fbb, const TriggerPacketT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const TriggerPacketT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _triggers = _o->triggers.size()
						 ? _fbb.CreateVector<flatbuffers::Offset<Trigger>>(_o->triggers.size(),
							   [](size_t i, _VectorArgs *__va) {
								   return CreateTrigger(*__va->__fbb, &__va->__o->triggers[i], __va->__rehasher);
							   },
							   &_va)
						 : 0;
	return dv::CreateTriggerPacket(_fbb, _triggers);
}

inline const flatbuffers::TypeTable *TriggerTypeTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
        {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
        {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
        {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {TriggerTypeTypeTable};
	static const char *const names[]
		= {"TIMESTAMP_RESET", "EXTERNAL_SIGNAL_RISING_EDGE", "EXTERNAL_SIGNAL_FALLING_EDGE", "EXTERNAL_SIGNAL_PULSE",
			"EXTERNAL_GENERATOR_RISING_EDGE", "EXTERNAL_GENERATOR_FALLING_EDGE", "APS_FRAME_START", "APS_FRAME_END",
			"APS_EXPOSURE_START", "APS_EXPOSURE_END"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 10, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *TriggerTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {TriggerTypeTypeTable};
	static const char *const names[]                   = {"timestamp", "type"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 2, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *TriggerPacketTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_SEQUENCE, 1, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {TriggerTypeTable};
	static const char *const names[]                   = {"triggers"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 1, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const dv::TriggerPacket *GetTriggerPacket(const void *buf) {
	return flatbuffers::GetRoot<dv::TriggerPacket>(buf);
}

inline const dv::TriggerPacket *GetSizePrefixedTriggerPacket(const void *buf) {
	return flatbuffers::GetSizePrefixedRoot<dv::TriggerPacket>(buf);
}

inline const char *TriggerPacketIdentifier() {
	return "TRIG";
}

const char *TriggerPacket::identifier = TriggerPacketIdentifier();

inline bool TriggerPacketBufferHasIdentifier(const void *buf) {
	return flatbuffers::BufferHasIdentifier(buf, TriggerPacketIdentifier());
}

inline bool VerifyTriggerPacketBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifyBuffer<dv::TriggerPacket>(TriggerPacketIdentifier());
}

inline bool VerifySizePrefixedTriggerPacketBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifySizePrefixedBuffer<dv::TriggerPacket>(TriggerPacketIdentifier());
}

inline void FinishTriggerPacketBuffer(
	flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<dv::TriggerPacket> root) {
	fbb.Finish(root, TriggerPacketIdentifier());
}

inline void FinishSizePrefixedTriggerPacketBuffer(
	flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<dv::TriggerPacket> root) {
	fbb.FinishSizePrefixed(root, TriggerPacketIdentifier());
}

inline std::unique_ptr<TriggerPacketT> UnPackTriggerPacket(
	const void *buf, const flatbuffers::resolver_function_t *res = nullptr) {
	return std::unique_ptr<TriggerPacketT>(GetTriggerPacket(buf)->UnPack(res));
}

} // namespace dv

#endif // FLATBUFFERS_GENERATED_TRIGGER_DV_H_
