// automatically generated by the FlatBuffers compiler, do not modify

#ifndef FLATBUFFERS_GENERATED_FRAME8_H_
#define FLATBUFFERS_GENERATED_FRAME8_H_

#include "cvector.hpp"
#include "flatbuffers/flatbuffers.h"

struct Frame8;
struct Frame8T;

struct Frame8Packet;
struct Frame8PacketT;

inline const flatbuffers::TypeTable *Frame8TypeTable();

inline const flatbuffers::TypeTable *Frame8PacketTypeTable();

enum class FrameChannels : int8_t {
	/// Grayscale, one channel only.
	GRAYSCALE = 1 /// Red Green Blue, 3 color channels.
	,
	RGB = 3 /// Red Green Blue Alpha, 3 color channels plus transparency.
	,
	RGBA = 4,
	MIN  = GRAYSCALE,
	MAX  = RGBA
};

inline const FrameChannels (&EnumValuesFrameChannels())[3] {
	static const FrameChannels values[] = {FrameChannels::GRAYSCALE, FrameChannels::RGB, FrameChannels::RGBA};
	return values;
}

inline const char *const *EnumNamesFrameChannels() {
	static const char *const names[] = {"GRAYSCALE", "", "RGB", "RGBA", nullptr};
	return names;
}

inline const char *EnumNameFrameChannels(FrameChannels e) {
	if (e < FrameChannels::GRAYSCALE || e > FrameChannels::RGBA)
		return "";
	const size_t index = static_cast<int>(e) - static_cast<int>(FrameChannels::GRAYSCALE);
	return EnumNamesFrameChannels()[index];
}

enum class FrameColorFilters : int8_t {
	/// No color filter present, all light passes.
	MONO = 0 /// Standard Bayer color filter, 1 red 2 green 1 blue. Variation 1.
	,
	RGBG = 1 /// Standard Bayer color filter, 1 red 2 green 1 blue. Variation 2.
	,
	GRGB = 2 /// Standard Bayer color filter, 1 red 2 green 1 blue. Variation 3.
	,
	GBGR = 3 /// Standard Bayer color filter, 1 red 2 green 1 blue. Variation 4.
	,
	BGRG = 4 /// Modified Bayer color filter, with white (pass all light) instead of extra green. Variation 1.
	,
	RGBW = 5 /// Modified Bayer color filter, with white (pass all light) instead of extra green. Variation 2.
	,
	GRWB = 6 /// Modified Bayer color filter, with white (pass all light) instead of extra green. Variation 3.
	,
	WBGR = 7 /// Modified Bayer color filter, with white (pass all light) instead of extra green. Variation 4.
	,
	BWRG = 8,
	MIN  = MONO,
	MAX  = BWRG
};

inline const FrameColorFilters (&EnumValuesFrameColorFilters())[9] {
	static const FrameColorFilters values[] = {FrameColorFilters::MONO, FrameColorFilters::RGBG,
		FrameColorFilters::GRGB, FrameColorFilters::GBGR, FrameColorFilters::BGRG, FrameColorFilters::RGBW,
		FrameColorFilters::GRWB, FrameColorFilters::WBGR, FrameColorFilters::BWRG};
	return values;
}

inline const char *const *EnumNamesFrameColorFilters() {
	static const char *const names[]
		= {"MONO", "RGBG", "GRGB", "GBGR", "BGRG", "RGBW", "GRWB", "WBGR", "BWRG", nullptr};
	return names;
}

inline const char *EnumNameFrameColorFilters(FrameColorFilters e) {
	if (e < FrameColorFilters::MONO || e > FrameColorFilters::BWRG)
		return "";
	const size_t index = static_cast<int>(e);
	return EnumNamesFrameColorFilters()[index];
}

struct Frame8T : public flatbuffers::NativeTable {
	typedef Frame8 TableType;
	int64_t timestamp;
	int64_t timestampStartOfFrame;
	int64_t timestampEndOfFrame;
	int64_t timestampStartOfExposure;
	int64_t timestampEndOfExposure;
	FrameChannels numChannels;
	FrameColorFilters origColorFilter;
	int16_t lengthX;
	int16_t lengthY;
	int16_t positionX;
	int16_t positionY;
	dv::cvector<uint8_t> pixels;
	Frame8T() :
		timestamp(0),
		timestampStartOfFrame(0),
		timestampEndOfFrame(0),
		timestampStartOfExposure(0),
		timestampEndOfExposure(0),
		numChannels(FrameChannels::GRAYSCALE),
		origColorFilter(FrameColorFilters::MONO),
		lengthX(0),
		lengthY(0),
		positionX(0),
		positionY(0) {
	}
};

struct Frame8 FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef Frame8T NativeTableType;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return Frame8TypeTable();
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
		VT_TIMESTAMP                = 4,
		VT_TIMESTAMPSTARTOFFRAME    = 6,
		VT_TIMESTAMPENDOFFRAME      = 8,
		VT_TIMESTAMPSTARTOFEXPOSURE = 10,
		VT_TIMESTAMPENDOFEXPOSURE   = 12,
		VT_NUMCHANNELS              = 14,
		VT_ORIGCOLORFILTER          = 16,
		VT_LENGTHX                  = 18,
		VT_LENGTHY                  = 20,
		VT_POSITIONX                = 22,
		VT_POSITIONY                = 24,
		VT_PIXELS                   = 26
	};
	/// Central timestamp, corresponds to exposure midpoint.
	int64_t timestamp() const {
		return GetField<int64_t>(VT_TIMESTAMP, 0);
	}
	/// Start of Frame (SOF) timestamp.
	int64_t timestampStartOfFrame() const {
		return GetField<int64_t>(VT_TIMESTAMPSTARTOFFRAME, 0);
	}
	/// End of Frame (EOF) timestamp.
	int64_t timestampEndOfFrame() const {
		return GetField<int64_t>(VT_TIMESTAMPENDOFFRAME, 0);
	}
	/// Start of Exposure (SOE) timestamp.
	int64_t timestampStartOfExposure() const {
		return GetField<int64_t>(VT_TIMESTAMPSTARTOFEXPOSURE, 0);
	}
	/// End of Exposure (EOE) timestamp.
	int64_t timestampEndOfExposure() const {
		return GetField<int64_t>(VT_TIMESTAMPENDOFEXPOSURE, 0);
	}
	/// Color channels present in frame.
	FrameChannels numChannels() const {
		return static_cast<FrameChannels>(GetField<int8_t>(VT_NUMCHANNELS, 1));
	}
	/// Original color filter on array.
	FrameColorFilters origColorFilter() const {
		return static_cast<FrameColorFilters>(GetField<int8_t>(VT_ORIGCOLORFILTER, 0));
	}
	/// X axis length in pixels.
	int16_t lengthX() const {
		return GetField<int16_t>(VT_LENGTHX, 0);
	}
	/// Y axis length in pixels.
	int16_t lengthY() const {
		return GetField<int16_t>(VT_LENGTHY, 0);
	}
	/// X axis position (upper left offset) in pixels.
	int16_t positionX() const {
		return GetField<int16_t>(VT_POSITIONX, 0);
	}
	/// Y axis position (upper left offset) in pixels.
	int16_t positionY() const {
		return GetField<int16_t>(VT_POSITIONY, 0);
	}
	/// Pixel values, 8bit depth.
	const flatbuffers::Vector<uint8_t> *pixels() const {
		return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_PIXELS);
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyField<int64_t>(verifier, VT_TIMESTAMP)
			   && VerifyField<int64_t>(verifier, VT_TIMESTAMPSTARTOFFRAME)
			   && VerifyField<int64_t>(verifier, VT_TIMESTAMPENDOFFRAME)
			   && VerifyField<int64_t>(verifier, VT_TIMESTAMPSTARTOFEXPOSURE)
			   && VerifyField<int64_t>(verifier, VT_TIMESTAMPENDOFEXPOSURE)
			   && VerifyField<int8_t>(verifier, VT_NUMCHANNELS) && VerifyField<int8_t>(verifier, VT_ORIGCOLORFILTER)
			   && VerifyField<int16_t>(verifier, VT_LENGTHX) && VerifyField<int16_t>(verifier, VT_LENGTHY)
			   && VerifyField<int16_t>(verifier, VT_POSITIONX) && VerifyField<int16_t>(verifier, VT_POSITIONY)
			   && VerifyOffset(verifier, VT_PIXELS) && verifier.VerifyVector(pixels()) && verifier.EndTable();
	}
	Frame8T *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(Frame8T *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static flatbuffers::Offset<Frame8> Pack(flatbuffers::FlatBufferBuilder &_fbb, const Frame8T *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct Frame8Builder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_timestamp(int64_t timestamp) {
		fbb_.AddElement<int64_t>(Frame8::VT_TIMESTAMP, timestamp, 0);
	}
	void add_timestampStartOfFrame(int64_t timestampStartOfFrame) {
		fbb_.AddElement<int64_t>(Frame8::VT_TIMESTAMPSTARTOFFRAME, timestampStartOfFrame, 0);
	}
	void add_timestampEndOfFrame(int64_t timestampEndOfFrame) {
		fbb_.AddElement<int64_t>(Frame8::VT_TIMESTAMPENDOFFRAME, timestampEndOfFrame, 0);
	}
	void add_timestampStartOfExposure(int64_t timestampStartOfExposure) {
		fbb_.AddElement<int64_t>(Frame8::VT_TIMESTAMPSTARTOFEXPOSURE, timestampStartOfExposure, 0);
	}
	void add_timestampEndOfExposure(int64_t timestampEndOfExposure) {
		fbb_.AddElement<int64_t>(Frame8::VT_TIMESTAMPENDOFEXPOSURE, timestampEndOfExposure, 0);
	}
	void add_numChannels(FrameChannels numChannels) {
		fbb_.AddElement<int8_t>(Frame8::VT_NUMCHANNELS, static_cast<int8_t>(numChannels), 1);
	}
	void add_origColorFilter(FrameColorFilters origColorFilter) {
		fbb_.AddElement<int8_t>(Frame8::VT_ORIGCOLORFILTER, static_cast<int8_t>(origColorFilter), 0);
	}
	void add_lengthX(int16_t lengthX) {
		fbb_.AddElement<int16_t>(Frame8::VT_LENGTHX, lengthX, 0);
	}
	void add_lengthY(int16_t lengthY) {
		fbb_.AddElement<int16_t>(Frame8::VT_LENGTHY, lengthY, 0);
	}
	void add_positionX(int16_t positionX) {
		fbb_.AddElement<int16_t>(Frame8::VT_POSITIONX, positionX, 0);
	}
	void add_positionY(int16_t positionY) {
		fbb_.AddElement<int16_t>(Frame8::VT_POSITIONY, positionY, 0);
	}
	void add_pixels(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> pixels) {
		fbb_.AddOffset(Frame8::VT_PIXELS, pixels);
	}
	explicit Frame8Builder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	Frame8Builder &operator=(const Frame8Builder &);
	flatbuffers::Offset<Frame8> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<Frame8>(end);
		return o;
	}
};

inline flatbuffers::Offset<Frame8> CreateFrame8(flatbuffers::FlatBufferBuilder &_fbb, int64_t timestamp = 0,
	int64_t timestampStartOfFrame = 0, int64_t timestampEndOfFrame = 0, int64_t timestampStartOfExposure = 0,
	int64_t timestampEndOfExposure = 0, FrameChannels numChannels = FrameChannels::GRAYSCALE,
	FrameColorFilters origColorFilter = FrameColorFilters::MONO, int16_t lengthX = 0, int16_t lengthY = 0,
	int16_t positionX = 0, int16_t positionY = 0, flatbuffers::Offset<flatbuffers::Vector<uint8_t>> pixels = 0) {
	Frame8Builder builder_(_fbb);
	builder_.add_timestampEndOfExposure(timestampEndOfExposure);
	builder_.add_timestampStartOfExposure(timestampStartOfExposure);
	builder_.add_timestampEndOfFrame(timestampEndOfFrame);
	builder_.add_timestampStartOfFrame(timestampStartOfFrame);
	builder_.add_timestamp(timestamp);
	builder_.add_pixels(pixels);
	builder_.add_positionY(positionY);
	builder_.add_positionX(positionX);
	builder_.add_lengthY(lengthY);
	builder_.add_lengthX(lengthX);
	builder_.add_origColorFilter(origColorFilter);
	builder_.add_numChannels(numChannels);
	return builder_.Finish();
}

inline flatbuffers::Offset<Frame8> CreateFrame8Direct(flatbuffers::FlatBufferBuilder &_fbb, int64_t timestamp = 0,
	int64_t timestampStartOfFrame = 0, int64_t timestampEndOfFrame = 0, int64_t timestampStartOfExposure = 0,
	int64_t timestampEndOfExposure = 0, FrameChannels numChannels = FrameChannels::GRAYSCALE,
	FrameColorFilters origColorFilter = FrameColorFilters::MONO, int16_t lengthX = 0, int16_t lengthY = 0,
	int16_t positionX = 0, int16_t positionY = 0, const std::vector<uint8_t> *pixels = nullptr) {
	auto pixels__ = pixels ? _fbb.CreateVector<uint8_t>(*pixels) : 0;
	return CreateFrame8(_fbb, timestamp, timestampStartOfFrame, timestampEndOfFrame, timestampStartOfExposure,
		timestampEndOfExposure, numChannels, origColorFilter, lengthX, lengthY, positionX, positionY, pixels__);
}

flatbuffers::Offset<Frame8> CreateFrame8(flatbuffers::FlatBufferBuilder &_fbb, const Frame8T *_o,
	const flatbuffers::rehasher_function_t *_rehasher = nullptr);

struct Frame8PacketT : public flatbuffers::NativeTable {
	typedef Frame8Packet TableType;
	dv::cvector<Frame8T> events;
	Frame8PacketT() {
	}
};

struct Frame8Packet FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef Frame8PacketT NativeTableType;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return Frame8PacketTypeTable();
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE { VT_EVENTS = 4 };
	const flatbuffers::Vector<flatbuffers::Offset<Frame8>> *events() const {
		return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Frame8>> *>(VT_EVENTS);
	}
	bool Verify(flatbuffers::Verifier &verifier) const {
		return VerifyTableStart(verifier) && VerifyOffset(verifier, VT_EVENTS) && verifier.VerifyVector(events())
			   && verifier.VerifyVectorOfTables(events()) && verifier.EndTable();
	}
	Frame8PacketT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(Frame8PacketT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static flatbuffers::Offset<Frame8Packet> Pack(flatbuffers::FlatBufferBuilder &_fbb, const Frame8PacketT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct Frame8PacketBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_events(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Frame8>>> events) {
		fbb_.AddOffset(Frame8Packet::VT_EVENTS, events);
	}
	explicit Frame8PacketBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	Frame8PacketBuilder &operator=(const Frame8PacketBuilder &);
	flatbuffers::Offset<Frame8Packet> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<Frame8Packet>(end);
		return o;
	}
};

inline flatbuffers::Offset<Frame8Packet> CreateFrame8Packet(flatbuffers::FlatBufferBuilder &_fbb,
	flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Frame8>>> events = 0) {
	Frame8PacketBuilder builder_(_fbb);
	builder_.add_events(events);
	return builder_.Finish();
}

inline flatbuffers::Offset<Frame8Packet> CreateFrame8PacketDirect(
	flatbuffers::FlatBufferBuilder &_fbb, const std::vector<flatbuffers::Offset<Frame8>> *events = nullptr) {
	auto events__ = events ? _fbb.CreateVector<flatbuffers::Offset<Frame8>>(*events) : 0;
	return CreateFrame8Packet(_fbb, events__);
}

flatbuffers::Offset<Frame8Packet> CreateFrame8Packet(flatbuffers::FlatBufferBuilder &_fbb, const Frame8PacketT *_o,
	const flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline Frame8T *Frame8::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new Frame8T();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void Frame8::UnPackTo(Frame8T *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	{
		auto _e       = timestamp();
		_o->timestamp = _e;
	};
	{
		auto _e                   = timestampStartOfFrame();
		_o->timestampStartOfFrame = _e;
	};
	{
		auto _e                 = timestampEndOfFrame();
		_o->timestampEndOfFrame = _e;
	};
	{
		auto _e                      = timestampStartOfExposure();
		_o->timestampStartOfExposure = _e;
	};
	{
		auto _e                    = timestampEndOfExposure();
		_o->timestampEndOfExposure = _e;
	};
	{
		auto _e         = numChannels();
		_o->numChannels = _e;
	};
	{
		auto _e             = origColorFilter();
		_o->origColorFilter = _e;
	};
	{
		auto _e     = lengthX();
		_o->lengthX = _e;
	};
	{
		auto _e     = lengthY();
		_o->lengthY = _e;
	};
	{
		auto _e       = positionX();
		_o->positionX = _e;
	};
	{
		auto _e       = positionY();
		_o->positionY = _e;
	};
	{
		auto _e = pixels();
		if (_e) {
			_o->pixels.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_o->pixels[_i] = _e->Get(_i);
			}
		}
	};
}

inline flatbuffers::Offset<Frame8> Frame8::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const Frame8T *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateFrame8(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<Frame8> CreateFrame8(
	flatbuffers::FlatBufferBuilder &_fbb, const Frame8T *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const Frame8T *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _timestamp                = _o->timestamp;
	auto _timestampStartOfFrame    = _o->timestampStartOfFrame;
	auto _timestampEndOfFrame      = _o->timestampEndOfFrame;
	auto _timestampStartOfExposure = _o->timestampStartOfExposure;
	auto _timestampEndOfExposure   = _o->timestampEndOfExposure;
	auto _numChannels              = _o->numChannels;
	auto _origColorFilter          = _o->origColorFilter;
	auto _lengthX                  = _o->lengthX;
	auto _lengthY                  = _o->lengthY;
	auto _positionX                = _o->positionX;
	auto _positionY                = _o->positionY;
	auto _pixels                   = _o->pixels.size() ? _fbb.CreateVector(_o->pixels.data(), _o->pixels.size()) : 0;
	return CreateFrame8(_fbb, _timestamp, _timestampStartOfFrame, _timestampEndOfFrame, _timestampStartOfExposure,
		_timestampEndOfExposure, _numChannels, _origColorFilter, _lengthX, _lengthY, _positionX, _positionY, _pixels);
}

inline Frame8PacketT *Frame8Packet::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new Frame8PacketT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void Frame8Packet::UnPackTo(Frame8PacketT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	{
		auto _e = events();
		if (_e) {
			_o->events.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_e->Get(_i)->UnPackTo(&_o->events[_i], _resolver);
			}
		}
	};
}

inline flatbuffers::Offset<Frame8Packet> Frame8Packet::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const Frame8PacketT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateFrame8Packet(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<Frame8Packet> CreateFrame8Packet(
	flatbuffers::FlatBufferBuilder &_fbb, const Frame8PacketT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const Frame8PacketT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _events = _o->events.size()
					   ? _fbb.CreateVector<flatbuffers::Offset<Frame8>>(_o->events.size(),
							 [](size_t i, _VectorArgs *__va) {
								 return CreateFrame8(*__va->__fbb, &__va->__o->events[i], __va->__rehasher);
							 },
							 &_va)
					   : 0;
	return CreateFrame8Packet(_fbb, _events);
}

inline const flatbuffers::TypeTable *FrameChannelsTypeTable() {
	static const flatbuffers::TypeCode type_codes[]
		= {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {FrameChannelsTypeTable};
	static const int64_t values[]                      = {1, 3, 4};
	static const char *const names[]                   = {"GRAYSCALE", "RGB", "RGBA"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 3, type_codes, type_refs, values, names};
	return &tt;
}

inline const flatbuffers::TypeTable *FrameColorFiltersTypeTable() {
	static const flatbuffers::TypeCode type_codes[]
		= {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {FrameColorFiltersTypeTable};
	static const char *const names[]       = {"MONO", "RGBG", "GRGB", "GBGR", "BGRG", "RGBW", "GRWB", "WBGR", "BWRG"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 9, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *Frame8TypeTable() {
	static const flatbuffers::TypeCode type_codes[]
		= {{flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_LONG, 0, -1},
			{flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_CHAR, 0, 0},
			{flatbuffers::ET_CHAR, 0, 1}, {flatbuffers::ET_SHORT, 0, -1}, {flatbuffers::ET_SHORT, 0, -1},
			{flatbuffers::ET_SHORT, 0, -1}, {flatbuffers::ET_SHORT, 0, -1}, {flatbuffers::ET_UCHAR, 1, -1}};
	static const flatbuffers::TypeFunction type_refs[] = {FrameChannelsTypeTable, FrameColorFiltersTypeTable};
	static const char *const names[]                   = {"timestamp", "timestampStartOfFrame", "timestampEndOfFrame",
        "timestampStartOfExposure", "timestampEndOfExposure", "numChannels", "origColorFilter", "lengthX", "lengthY",
        "positionX", "positionY", "pixels"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 12, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const flatbuffers::TypeTable *Frame8PacketTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_SEQUENCE, 1, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {Frame8TypeTable};
	static const char *const names[]                   = {"events"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 1, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const Frame8Packet *GetFrame8Packet(const void *buf) {
	return flatbuffers::GetRoot<Frame8Packet>(buf);
}

inline const Frame8Packet *GetSizePrefixedFrame8Packet(const void *buf) {
	return flatbuffers::GetSizePrefixedRoot<Frame8Packet>(buf);
}

inline const char *Frame8PacketIdentifier() {
	return "FRM8";
}

inline bool Frame8PacketBufferHasIdentifier(const void *buf) {
	return flatbuffers::BufferHasIdentifier(buf, Frame8PacketIdentifier());
}

inline bool VerifyFrame8PacketBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifyBuffer<Frame8Packet>(Frame8PacketIdentifier());
}

inline bool VerifySizePrefixedFrame8PacketBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifySizePrefixedBuffer<Frame8Packet>(Frame8PacketIdentifier());
}

inline void FinishFrame8PacketBuffer(flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<Frame8Packet> root) {
	fbb.Finish(root, Frame8PacketIdentifier());
}

inline void FinishSizePrefixedFrame8PacketBuffer(
	flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<Frame8Packet> root) {
	fbb.FinishSizePrefixed(root, Frame8PacketIdentifier());
}

inline std::unique_ptr<Frame8PacketT> UnPackFrame8Packet(
	const void *buf, const flatbuffers::resolver_function_t *res = nullptr) {
	return std::unique_ptr<Frame8PacketT>(GetFrame8Packet(buf)->UnPack(res));
}

#endif // FLATBUFFERS_GENERATED_FRAME8_H_
