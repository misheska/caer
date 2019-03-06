// automatically generated by the FlatBuffers compiler, do not modify

#ifndef FLATBUFFERS_GENERATED_TEST_H_
#define FLATBUFFERS_GENERATED_TEST_H_

#include "cstring.hpp"
#include "cvector.hpp"
#include "flatbuffers/flatbuffers.h"

struct TestStruct;

struct TestTable;
struct TestTableT;

struct Test;
struct TestT;

struct TestPacket;
struct TestPacketT;

inline const flatbuffers::TypeTable *TestStructTypeTable();

inline const flatbuffers::TypeTable *TestTableTypeTable();

inline const flatbuffers::TypeTable *TestTypeTable();

inline const flatbuffers::TypeTable *TestPacketTypeTable();

FLATBUFFERS_MANUALLY_ALIGNED_STRUCT(4) TestStruct FLATBUFFERS_FINAL_CLASS {
private:
	int32_t num_;
	uint8_t valid_;
	int8_t padding0__;
	int16_t padding1__;

public:
	TestStruct() {
		memset(this, 0, sizeof(TestStruct));
	}
	TestStruct(int32_t _num, bool _valid) :
		num_(flatbuffers::EndianScalar(_num)),
		valid_(flatbuffers::EndianScalar(static_cast<uint8_t>(_valid))),
		padding0__(0),
		padding1__(0) {
		(void) padding0__;
		(void) padding1__;
	}
	int32_t num() const {
		return flatbuffers::EndianScalar(num_);
	}
	bool valid() const {
		return flatbuffers::EndianScalar(valid_) != 0;
	}
};
FLATBUFFERS_STRUCT_END(TestStruct, 8);

struct TestTableT : public flatbuffers::NativeTable {
	typedef TestTable TableType;
	float length;
	dv::cstring node;
	TestTableT() : length(0.0f) {
	}
};

struct TestTable FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef TestTableT NativeTableType;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return TestTableTypeTable();
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE { VT_LENGTH = 4, VT_NODE = 6 };
	float length() const {
		return GetField<float>(VT_LENGTH, 0.0f);
	}
	const flatbuffers::String *node() const {
		return GetPointer<const flatbuffers::String *>(VT_NODE);
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyField<float>(verifier, VT_LENGTH) && VerifyOffset(verifier, VT_NODE)
			   && verifier.VerifyString(node()) && verifier.EndTable();
	}
	TestTableT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(TestTableT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static flatbuffers::Offset<TestTable> Pack(flatbuffers::FlatBufferBuilder &_fbb, const TestTableT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct TestTableBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_length(float length) {
		fbb_.AddElement<float>(TestTable::VT_LENGTH, length, 0.0f);
	}
	void add_node(flatbuffers::Offset<flatbuffers::String> node) {
		fbb_.AddOffset(TestTable::VT_NODE, node);
	}
	explicit TestTableBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	TestTableBuilder &operator=(const TestTableBuilder &);
	flatbuffers::Offset<TestTable> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<TestTable>(end);
		return o;
	}
};

inline flatbuffers::Offset<TestTable> CreateTestTable(
	flatbuffers::FlatBufferBuilder &_fbb, float length = 0.0f, flatbuffers::Offset<flatbuffers::String> node = 0) {
	TestTableBuilder builder_(_fbb);
	builder_.add_node(node);
	builder_.add_length(length);
	return builder_.Finish();
}

inline flatbuffers::Offset<TestTable> CreateTestTableDirect(
	flatbuffers::FlatBufferBuilder &_fbb, float length = 0.0f, const char *node = nullptr) {
	auto node__ = node ? _fbb.CreateString(node) : 0;
	return CreateTestTable(_fbb, length, node__);
}

flatbuffers::Offset<TestTable> CreateTestTable(flatbuffers::FlatBufferBuilder &_fbb, const TestTableT *_o,
	const flatbuffers::rehasher_function_t *_rehasher = nullptr);

struct TestT : public flatbuffers::NativeTable {
	typedef Test TableType;
	int64_t timestamp;
	int16_t addressX;
	int16_t addressY;
	bool polarity;
	dv::cstring astr;
	dv::cvector<bool> aboolvec;
	dv::cvector<int32_t> aintvec;
	dv::cvector<dv::cstring> astrvec;
	TestTableT ttab;
	TestStruct tstru;
	dv::cvector<TestTableT> ttabvec;
	dv::cvector<TestStruct> tstruvec;
	TestT() : timestamp(0), addressX(0), addressY(0), polarity(false) {
	}
};

struct Test FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef TestT NativeTableType;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return TestTypeTable();
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
		VT_TIMESTAMP = 4,
		VT_ADDRESSX  = 6,
		VT_ADDRESSY  = 8,
		VT_POLARITY  = 10,
		VT_ASTR      = 12,
		VT_ABOOLVEC  = 14,
		VT_AINTVEC   = 16,
		VT_ASTRVEC   = 18,
		VT_TTAB      = 20,
		VT_TSTRU     = 22,
		VT_TTABVEC   = 24,
		VT_TSTRUVEC  = 26
	};
	int64_t timestamp() const {
		return GetField<int64_t>(VT_TIMESTAMP, 0);
	}
	int16_t addressX() const {
		return GetField<int16_t>(VT_ADDRESSX, 0);
	}
	int16_t addressY() const {
		return GetField<int16_t>(VT_ADDRESSY, 0);
	}
	bool polarity() const {
		return GetField<uint8_t>(VT_POLARITY, 0) != 0;
	}
	const flatbuffers::String *astr() const {
		return GetPointer<const flatbuffers::String *>(VT_ASTR);
	}
	const flatbuffers::Vector<uint8_t> *aboolvec() const {
		return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_ABOOLVEC);
	}
	const flatbuffers::Vector<int32_t> *aintvec() const {
		return GetPointer<const flatbuffers::Vector<int32_t> *>(VT_AINTVEC);
	}
	const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>> *astrvec() const {
		return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>> *>(VT_ASTRVEC);
	}
	const TestTable *ttab() const {
		return GetPointer<const TestTable *>(VT_TTAB);
	}
	const TestStruct *tstru() const {
		return GetStruct<const TestStruct *>(VT_TSTRU);
	}
	const flatbuffers::Vector<flatbuffers::Offset<TestTable>> *ttabvec() const {
		return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<TestTable>> *>(VT_TTABVEC);
	}
	const flatbuffers::Vector<const TestStruct *> *tstruvec() const {
		return GetPointer<const flatbuffers::Vector<const TestStruct *> *>(VT_TSTRUVEC);
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyField<int64_t>(verifier, VT_TIMESTAMP)
			   && VerifyField<int16_t>(verifier, VT_ADDRESSX) && VerifyField<int16_t>(verifier, VT_ADDRESSY)
			   && VerifyField<uint8_t>(verifier, VT_POLARITY) && VerifyOffset(verifier, VT_ASTR)
			   && verifier.VerifyString(astr()) && VerifyOffset(verifier, VT_ABOOLVEC)
			   && verifier.VerifyVector(aboolvec()) && VerifyOffset(verifier, VT_AINTVEC)
			   && verifier.VerifyVector(aintvec()) && VerifyOffset(verifier, VT_ASTRVEC)
			   && verifier.VerifyVector(astrvec()) && verifier.VerifyVectorOfStrings(astrvec())
			   && VerifyOffset(verifier, VT_TTAB) && verifier.VerifyTable(ttab())
			   && VerifyField<TestStruct>(verifier, VT_TSTRU) && VerifyOffset(verifier, VT_TTABVEC)
			   && verifier.VerifyVector(ttabvec()) && verifier.VerifyVectorOfTables(ttabvec())
			   && VerifyOffset(verifier, VT_TSTRUVEC) && verifier.VerifyVector(tstruvec()) && verifier.EndTable();
	}
	TestT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(TestT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static flatbuffers::Offset<Test> Pack(flatbuffers::FlatBufferBuilder &_fbb, const TestT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct TestBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_timestamp(int64_t timestamp) {
		fbb_.AddElement<int64_t>(Test::VT_TIMESTAMP, timestamp, 0);
	}
	void add_addressX(int16_t addressX) {
		fbb_.AddElement<int16_t>(Test::VT_ADDRESSX, addressX, 0);
	}
	void add_addressY(int16_t addressY) {
		fbb_.AddElement<int16_t>(Test::VT_ADDRESSY, addressY, 0);
	}
	void add_polarity(bool polarity) {
		fbb_.AddElement<uint8_t>(Test::VT_POLARITY, static_cast<uint8_t>(polarity), 0);
	}
	void add_astr(flatbuffers::Offset<flatbuffers::String> astr) {
		fbb_.AddOffset(Test::VT_ASTR, astr);
	}
	void add_aboolvec(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> aboolvec) {
		fbb_.AddOffset(Test::VT_ABOOLVEC, aboolvec);
	}
	void add_aintvec(flatbuffers::Offset<flatbuffers::Vector<int32_t>> aintvec) {
		fbb_.AddOffset(Test::VT_AINTVEC, aintvec);
	}
	void add_astrvec(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> astrvec) {
		fbb_.AddOffset(Test::VT_ASTRVEC, astrvec);
	}
	void add_ttab(flatbuffers::Offset<TestTable> ttab) {
		fbb_.AddOffset(Test::VT_TTAB, ttab);
	}
	void add_tstru(const TestStruct *tstru) {
		fbb_.AddStruct(Test::VT_TSTRU, tstru);
	}
	void add_ttabvec(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<TestTable>>> ttabvec) {
		fbb_.AddOffset(Test::VT_TTABVEC, ttabvec);
	}
	void add_tstruvec(flatbuffers::Offset<flatbuffers::Vector<const TestStruct *>> tstruvec) {
		fbb_.AddOffset(Test::VT_TSTRUVEC, tstruvec);
	}
	explicit TestBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	TestBuilder &operator=(const TestBuilder &);
	flatbuffers::Offset<Test> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<Test>(end);
		return o;
	}
};

inline flatbuffers::Offset<Test> CreateTest(flatbuffers::FlatBufferBuilder &_fbb, int64_t timestamp = 0,
	int16_t addressX = 0, int16_t addressY = 0, bool polarity = false,
	flatbuffers::Offset<flatbuffers::String> astr = 0, flatbuffers::Offset<flatbuffers::Vector<uint8_t>> aboolvec = 0,
	flatbuffers::Offset<flatbuffers::Vector<int32_t>> aintvec                                  = 0,
	flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> astrvec = 0,
	flatbuffers::Offset<TestTable> ttab = 0, const TestStruct *tstru = 0,
	flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<TestTable>>> ttabvec = 0,
	flatbuffers::Offset<flatbuffers::Vector<const TestStruct *>> tstruvec            = 0) {
	TestBuilder builder_(_fbb);
	builder_.add_timestamp(timestamp);
	builder_.add_tstruvec(tstruvec);
	builder_.add_ttabvec(ttabvec);
	builder_.add_tstru(tstru);
	builder_.add_ttab(ttab);
	builder_.add_astrvec(astrvec);
	builder_.add_aintvec(aintvec);
	builder_.add_aboolvec(aboolvec);
	builder_.add_astr(astr);
	builder_.add_addressY(addressY);
	builder_.add_addressX(addressX);
	builder_.add_polarity(polarity);
	return builder_.Finish();
}

inline flatbuffers::Offset<Test> CreateTestDirect(flatbuffers::FlatBufferBuilder &_fbb, int64_t timestamp = 0,
	int16_t addressX = 0, int16_t addressY = 0, bool polarity = false, const char *astr = nullptr,
	const std::vector<uint8_t> *aboolvec = nullptr, const std::vector<int32_t> *aintvec = nullptr,
	const std::vector<flatbuffers::Offset<flatbuffers::String>> *astrvec = nullptr,
	flatbuffers::Offset<TestTable> ttab = 0, const TestStruct *tstru = 0,
	const std::vector<flatbuffers::Offset<TestTable>> *ttabvec = nullptr,
	const std::vector<TestStruct> *tstruvec                    = nullptr) {
	auto astr__     = astr ? _fbb.CreateString(astr) : 0;
	auto aboolvec__ = aboolvec ? _fbb.CreateVector<uint8_t>(*aboolvec) : 0;
	auto aintvec__  = aintvec ? _fbb.CreateVector<int32_t>(*aintvec) : 0;
	auto astrvec__  = astrvec ? _fbb.CreateVector<flatbuffers::Offset<flatbuffers::String>>(*astrvec) : 0;
	auto ttabvec__  = ttabvec ? _fbb.CreateVector<flatbuffers::Offset<TestTable>>(*ttabvec) : 0;
	auto tstruvec__ = tstruvec ? _fbb.CreateVectorOfStructs<TestStruct>(*tstruvec) : 0;
	return CreateTest(_fbb, timestamp, addressX, addressY, polarity, astr__, aboolvec__, aintvec__, astrvec__, ttab,
		tstru, ttabvec__, tstruvec__);
}

flatbuffers::Offset<Test> CreateTest(
	flatbuffers::FlatBufferBuilder &_fbb, const TestT *_o, const flatbuffers::rehasher_function_t *_rehasher = nullptr);

struct TestPacketT : public flatbuffers::NativeTable {
	typedef TestPacket TableType;
	dv::cvector<std::unique_ptr<TestT>> events;
	TestPacketT() {
	}
};

struct TestPacket FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef TestPacketT NativeTableType;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return TestPacketTypeTable();
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE { VT_EVENTS = 4 };
	const flatbuffers::Vector<flatbuffers::Offset<Test>> *events() const {
		return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Test>> *>(VT_EVENTS);
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyOffset(verifier, VT_EVENTS) && verifier.VerifyVector(events())
			   && verifier.VerifyVectorOfTables(events()) && verifier.EndTable();
	}
	TestPacketT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(TestPacketT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static flatbuffers::Offset<TestPacket> Pack(flatbuffers::FlatBufferBuilder &_fbb, const TestPacketT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct TestPacketBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_events(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Test>>> events) {
		fbb_.AddOffset(TestPacket::VT_EVENTS, events);
	}
	explicit TestPacketBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	TestPacketBuilder &operator=(const TestPacketBuilder &);
	flatbuffers::Offset<TestPacket> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<TestPacket>(end);
		return o;
	}
};

inline flatbuffers::Offset<TestPacket> CreateTestPacket(flatbuffers::FlatBufferBuilder &_fbb,
	flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Test>>> events = 0) {
	TestPacketBuilder builder_(_fbb);
	builder_.add_events(events);
	return builder_.Finish();
}

inline flatbuffers::Offset<TestPacket> CreateTestPacketDirect(
	flatbuffers::FlatBufferBuilder &_fbb, const std::vector<flatbuffers::Offset<Test>> *events = nullptr) {
	auto events__ = events ? _fbb.CreateVector<flatbuffers::Offset<Test>>(*events) : 0;
	return CreateTestPacket(_fbb, events__);
}

flatbuffers::Offset<TestPacket> CreateTestPacket(flatbuffers::FlatBufferBuilder &_fbb, const TestPacketT *_o,
	const flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline TestTableT *TestTable::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new TestTableT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void TestTable::UnPackTo(TestTableT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	{
		auto _e    = length();
		_o->length = _e;
	};
	{
		auto _e = node();
		if (_e)
			_o->node = dv::cstring(_e->c_str(), _e->size());
	};
}

inline flatbuffers::Offset<TestTable> TestTable::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const TestTableT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateTestTable(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<TestTable> CreateTestTable(
	flatbuffers::FlatBufferBuilder &_fbb, const TestTableT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const TestTableT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _length = _o->length;
	auto _node   = _o->node.empty() ? 0 : _fbb.CreateString(_o->node);
	return CreateTestTable(_fbb, _length, _node);
}

inline TestT *Test::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new TestT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void Test::UnPackTo(TestT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	{
		auto _e       = timestamp();
		_o->timestamp = _e;
	};
	{
		auto _e      = addressX();
		_o->addressX = _e;
	};
	{
		auto _e      = addressY();
		_o->addressY = _e;
	};
	{
		auto _e      = polarity();
		_o->polarity = _e;
	};
	{
		auto _e = astr();
		if (_e)
			_o->astr = dv::cstring(_e->c_str(), _e->size());
	};
	{
		auto _e = aboolvec();
		if (_e) {
			_o->aboolvec.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_o->aboolvec[_i] = _e->Get(_i) != 0;
			}
		}
	};
	{
		auto _e = aintvec();
		if (_e) {
			_o->aintvec.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_o->aintvec[_i] = _e->Get(_i);
			}
		}
	};
	{
		auto _e = astrvec();
		if (_e) {
			_o->astrvec.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_o->astrvec[_i] = dv::cstring(_e->Get(_i)->c_str(), _e->Get(_i)->size());
			}
		}
	};
	{
		auto _e = ttab();
		if (_e)
			_e->UnPackTo(&_o->ttab, _resolver);
	};
	{
		auto _e = tstru();
		if (_e)
			_o->tstru = *_e;
	};
	{
		auto _e = ttabvec();
		if (_e) {
			_o->ttabvec.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_e->Get(_i)->UnPackTo(&_o->ttabvec[_i], _resolver);
			}
		}
	};
	{
		auto _e = tstruvec();
		if (_e) {
			_o->tstruvec.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_o->tstruvec[_i] = *_e->Get(_i);
			}
		}
	};
}

inline flatbuffers::Offset<Test> Test::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const TestT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateTest(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<Test> CreateTest(
	flatbuffers::FlatBufferBuilder &_fbb, const TestT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const TestT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _timestamp = _o->timestamp;
	auto _addressX  = _o->addressX;
	auto _addressY  = _o->addressY;
	auto _polarity  = _o->polarity;
	auto _astr      = _o->astr.empty() ? 0 : _fbb.CreateString(_o->astr);
	auto _aboolvec  = _o->aboolvec.size()
						 ? _fbb.CreateVectorScalarCast<uint8_t, bool>(_o->aboolvec.data(), _o->aboolvec.size())
						 : 0;
	auto _aintvec = _o->aintvec.size() ? _fbb.CreateVector(_o->aintvec.data(), _o->aintvec.size()) : 0;
	auto _astrvec
		= _o->astrvec.size()
			  ? _fbb.CreateVector<flatbuffers::Offset<flatbuffers::String>>(_o->astrvec.size(),
					[](size_t i, _VectorArgs *__va) { return __va->__fbb->CreateString(__va->__o->astrvec[i]); }, &_va)
			  : 0;
	auto _ttab    = CreateTestTable(_fbb, &_o->ttab, _rehasher);
	auto _tstru   = &_o->tstru;
	auto _ttabvec = _o->ttabvec.size()
						? _fbb.CreateVector<flatbuffers::Offset<TestTable>>(_o->ttabvec.size(),
							  [](size_t i, _VectorArgs *__va) {
								  return CreateTestTable(*__va->__fbb, &__va->__o->ttabvec[i], __va->__rehasher);
							  },
							  &_va)
						: 0;
	auto _tstruvec = _o->tstruvec.size() ? _fbb.CreateVectorOfStructs(_o->tstruvec.data(), _o->tstruvec.size()) : 0;
	return CreateTest(_fbb, _timestamp, _addressX, _addressY, _polarity, _astr, _aboolvec, _aintvec, _astrvec, _ttab,
		_tstru, _ttabvec, _tstruvec);
}

inline TestPacketT *TestPacket::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new TestPacketT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void TestPacket::UnPackTo(TestPacketT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	{
		auto _e = events();
		if (_e) {
			_o->events.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_o->events[_i] = std::unique_ptr<TestT>(_e->Get(_i)->UnPack(_resolver));
			}
		}
	};
}

inline flatbuffers::Offset<TestPacket> TestPacket::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const TestPacketT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateTestPacket(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<TestPacket> CreateTestPacket(
	flatbuffers::FlatBufferBuilder &_fbb, const TestPacketT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const TestPacketT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _events = _o->events.size()
					   ? _fbb.CreateVector<flatbuffers::Offset<Test>>(_o->events.size(),
							 [](size_t i, _VectorArgs *__va) {
								 return CreateTest(*__va->__fbb, __va->__o->events[i].get(), __va->__rehasher);
							 },
							 &_va)
					   : 0;
	return CreateTestPacket(_fbb, _events);
}

inline const flatbuffers::TypeTable *TestStructTypeTable() {
	static const flatbuffers::TypeCode type_codes[] = {{flatbuffers::ET_INT, 0, -1}, {flatbuffers::ET_BOOL, 0, -1}};
	static const int64_t values[]                   = {0, 4, 8};
	static const char *const names[]                = {"num", "valid"};
	static const flatbuffers::TypeTable tt          = {flatbuffers::ST_STRUCT, 2, type_codes, nullptr, values, names};
	return &tt;
}

inline const flatbuffers::TypeTable *TestTableTypeTable() {
	static const flatbuffers::TypeCode type_codes[] = {{flatbuffers::ET_FLOAT, 0, -1}, {flatbuffers::ET_STRING, 0, -1}};
	static const char *const names[]                = {"length", "node"};
	static const flatbuffers::TypeTable tt          = {flatbuffers::ST_TABLE, 2, type_codes, nullptr, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *TestTypeTable() {
	static const flatbuffers::TypeCode type_codes[]
		= {{flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_SHORT, 0, -1}, {flatbuffers::ET_SHORT, 0, -1},
			{flatbuffers::ET_BOOL, 0, -1}, {flatbuffers::ET_STRING, 0, -1}, {flatbuffers::ET_BOOL, 1, -1},
			{flatbuffers::ET_INT, 1, -1}, {flatbuffers::ET_STRING, 1, -1}, {flatbuffers::ET_SEQUENCE, 0, 0},
			{flatbuffers::ET_SEQUENCE, 0, 1}, {flatbuffers::ET_SEQUENCE, 1, 0}, {flatbuffers::ET_SEQUENCE, 1, 1}};
	static const flatbuffers::TypeFunction type_refs[] = {TestTableTypeTable, TestStructTypeTable};
	static const char *const names[] = {"timestamp", "addressX", "addressY", "polarity", "astr", "aboolvec", "aintvec",
		"astrvec", "ttab", "tstru", "ttabvec", "tstruvec"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 12, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *TestPacketTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_SEQUENCE, 1, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {TestTypeTable};
	static const char *const names[]                   = {"events"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 1, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const TestPacket *GetTestPacket(const void *buf) {
	return flatbuffers::GetRoot<TestPacket>(buf);
}

inline const TestPacket *GetSizePrefixedTestPacket(const void *buf) {
	return flatbuffers::GetSizePrefixedRoot<TestPacket>(buf);
}

inline const char *TestPacketIdentifier() {
	return "TEST";
}

inline bool TestPacketBufferHasIdentifier(const void *buf) {
	return flatbuffers::BufferHasIdentifier(buf, TestPacketIdentifier());
}

inline bool VerifyTestPacketBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifyBuffer<TestPacket>(TestPacketIdentifier());
}

inline bool VerifySizePrefixedTestPacketBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifySizePrefixedBuffer<TestPacket>(TestPacketIdentifier());
}

inline void FinishTestPacketBuffer(flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<TestPacket> root) {
	fbb.Finish(root, TestPacketIdentifier());
}

inline void FinishSizePrefixedTestPacketBuffer(
	flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<TestPacket> root) {
	fbb.FinishSizePrefixed(root, TestPacketIdentifier());
}

inline std::unique_ptr<TestPacketT> UnPackTestPacket(
	const void *buf, const flatbuffers::resolver_function_t *res = nullptr) {
	return std::unique_ptr<TestPacketT>(GetTestPacket(buf)->UnPack(res));
}

#endif // FLATBUFFERS_GENERATED_TEST_H_
