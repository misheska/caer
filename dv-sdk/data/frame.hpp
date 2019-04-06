// automatically generated by the FlatBuffers compiler, do not modify

#ifndef FLATBUFFERS_GENERATED_FRAME_DV_H_
#define FLATBUFFERS_GENERATED_FRAME_DV_H_

#include "cvector.hpp"
#include "flatbuffers/flatbuffers.h"

namespace dv {

struct Frame;
struct FrameT;

inline const flatbuffers::TypeTable *FrameTypeTable();

/// Format values are compatible with OpenCV CV_8UCx.
/// Pixel layout follows OpenCV standard.
enum class FrameFormat : int8_t {
	/// Grayscale, one channel only.
	GRAY = 0 /// Blue Green Red, 3 color channels.
	,
	BGR = 16 /// Blue Green Red Alpha, 3 color channels plus transparency.
	,
	BGRA = 24,
	MIN  = GRAY,
	MAX  = BGRA
};

inline const FrameFormat (&EnumValuesFrameFormat())[3] {
	static const FrameFormat values[] = {FrameFormat::GRAY, FrameFormat::BGR, FrameFormat::BGRA};
	return values;
}

inline const char *EnumNameFrameFormat(FrameFormat e) {
	switch (e) {
		case FrameFormat::GRAY:
			return "GRAY";
		case FrameFormat::BGR:
			return "BGR";
		case FrameFormat::BGRA:
			return "BGRA";
		default:
			return "";
	}
}

struct FrameT : public flatbuffers::NativeTable {
	typedef Frame TableType;
	int64_t timestamp;
	int64_t timestampStartOfFrame;
	int64_t timestampEndOfFrame;
	int64_t timestampStartOfExposure;
	int64_t timestampEndOfExposure;
	FrameFormat format;
	int16_t sizeX;
	int16_t sizeY;
	int16_t positionX;
	int16_t positionY;
	dv::cvector<uint8_t> pixels;
	FrameT() :
		timestamp(0),
		timestampStartOfFrame(0),
		timestampEndOfFrame(0),
		timestampStartOfExposure(0),
		timestampEndOfExposure(0),
		format(FrameFormat::GRAY),
		sizeX(0),
		sizeY(0),
		positionX(0),
		positionY(0) {
	}
};

struct Frame FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
	typedef FrameT NativeTableType;
	static const char *identifier;
	static const flatbuffers::TypeTable *MiniReflectTypeTable() {
		return FrameTypeTable();
	}
	enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
		VT_TIMESTAMP                = 4,
		VT_TIMESTAMPSTARTOFFRAME    = 6,
		VT_TIMESTAMPENDOFFRAME      = 8,
		VT_TIMESTAMPSTARTOFEXPOSURE = 10,
		VT_TIMESTAMPENDOFEXPOSURE   = 12,
		VT_FORMAT                   = 14,
		VT_SIZEX                    = 16,
		VT_SIZEY                    = 18,
		VT_POSITIONX                = 20,
		VT_POSITIONY                = 22,
		VT_PIXELS                   = 24
	};
	/// Central timestamp (µs), corresponds to exposure midpoint.
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
	/// Pixel format (grayscale, RGB, ...).
	FrameFormat format() const {
		return static_cast<FrameFormat>(GetField<int8_t>(VT_FORMAT, 0));
	}
	/// X axis length in pixels.
	int16_t sizeX() const {
		return GetField<int16_t>(VT_SIZEX, 0);
	}
	/// Y axis length in pixels.
	int16_t sizeY() const {
		return GetField<int16_t>(VT_SIZEY, 0);
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
			   && VerifyField<int64_t>(verifier, VT_TIMESTAMPENDOFEXPOSURE) && VerifyField<int8_t>(verifier, VT_FORMAT)
			   && VerifyField<int16_t>(verifier, VT_SIZEX) && VerifyField<int16_t>(verifier, VT_SIZEY)
			   && VerifyField<int16_t>(verifier, VT_POSITIONX) && VerifyField<int16_t>(verifier, VT_POSITIONY)
			   && VerifyOffset(verifier, VT_PIXELS) && verifier.VerifyVector(pixels()) && verifier.EndTable();
	}
	FrameT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	void UnPackTo(FrameT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
	static void UnPackToFrom(FrameT *_o, const Frame *_fb, const flatbuffers::resolver_function_t *_resolver = nullptr);
	static flatbuffers::Offset<Frame> Pack(flatbuffers::FlatBufferBuilder &_fbb, const FrameT *_o,
		const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct FrameBuilder {
	flatbuffers::FlatBufferBuilder &fbb_;
	flatbuffers::uoffset_t start_;
	void add_timestamp(int64_t timestamp) {
		fbb_.AddElement<int64_t>(Frame::VT_TIMESTAMP, timestamp, 0);
	}
	void add_timestampStartOfFrame(int64_t timestampStartOfFrame) {
		fbb_.AddElement<int64_t>(Frame::VT_TIMESTAMPSTARTOFFRAME, timestampStartOfFrame, 0);
	}
	void add_timestampEndOfFrame(int64_t timestampEndOfFrame) {
		fbb_.AddElement<int64_t>(Frame::VT_TIMESTAMPENDOFFRAME, timestampEndOfFrame, 0);
	}
	void add_timestampStartOfExposure(int64_t timestampStartOfExposure) {
		fbb_.AddElement<int64_t>(Frame::VT_TIMESTAMPSTARTOFEXPOSURE, timestampStartOfExposure, 0);
	}
	void add_timestampEndOfExposure(int64_t timestampEndOfExposure) {
		fbb_.AddElement<int64_t>(Frame::VT_TIMESTAMPENDOFEXPOSURE, timestampEndOfExposure, 0);
	}
	void add_format(FrameFormat format) {
		fbb_.AddElement<int8_t>(Frame::VT_FORMAT, static_cast<int8_t>(format), 0);
	}
	void add_sizeX(int16_t sizeX) {
		fbb_.AddElement<int16_t>(Frame::VT_SIZEX, sizeX, 0);
	}
	void add_sizeY(int16_t sizeY) {
		fbb_.AddElement<int16_t>(Frame::VT_SIZEY, sizeY, 0);
	}
	void add_positionX(int16_t positionX) {
		fbb_.AddElement<int16_t>(Frame::VT_POSITIONX, positionX, 0);
	}
	void add_positionY(int16_t positionY) {
		fbb_.AddElement<int16_t>(Frame::VT_POSITIONY, positionY, 0);
	}
	void add_pixels(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> pixels) {
		fbb_.AddOffset(Frame::VT_PIXELS, pixels);
	}
	explicit FrameBuilder(flatbuffers::FlatBufferBuilder &_fbb) : fbb_(_fbb) {
		start_ = fbb_.StartTable();
	}
	FrameBuilder &operator=(const FrameBuilder &);
	flatbuffers::Offset<Frame> Finish() {
		const auto end = fbb_.EndTable(start_);
		auto o         = flatbuffers::Offset<Frame>(end);
		return o;
	}
};

inline flatbuffers::Offset<Frame> CreateFrame(flatbuffers::FlatBufferBuilder &_fbb, int64_t timestamp = 0,
	int64_t timestampStartOfFrame = 0, int64_t timestampEndOfFrame = 0, int64_t timestampStartOfExposure = 0,
	int64_t timestampEndOfExposure = 0, FrameFormat format = FrameFormat::GRAY, int16_t sizeX = 0, int16_t sizeY = 0,
	int16_t positionX = 0, int16_t positionY = 0, flatbuffers::Offset<flatbuffers::Vector<uint8_t>> pixels = 0) {
	FrameBuilder builder_(_fbb);
	builder_.add_timestampEndOfExposure(timestampEndOfExposure);
	builder_.add_timestampStartOfExposure(timestampStartOfExposure);
	builder_.add_timestampEndOfFrame(timestampEndOfFrame);
	builder_.add_timestampStartOfFrame(timestampStartOfFrame);
	builder_.add_timestamp(timestamp);
	builder_.add_pixels(pixels);
	builder_.add_positionY(positionY);
	builder_.add_positionX(positionX);
	builder_.add_sizeY(sizeY);
	builder_.add_sizeX(sizeX);
	builder_.add_format(format);
	return builder_.Finish();
}

inline flatbuffers::Offset<Frame> CreateFrameDirect(flatbuffers::FlatBufferBuilder &_fbb, int64_t timestamp = 0,
	int64_t timestampStartOfFrame = 0, int64_t timestampEndOfFrame = 0, int64_t timestampStartOfExposure = 0,
	int64_t timestampEndOfExposure = 0, FrameFormat format = FrameFormat::GRAY, int16_t sizeX = 0, int16_t sizeY = 0,
	int16_t positionX = 0, int16_t positionY = 0, const std::vector<uint8_t> *pixels = nullptr) {
	auto pixels__ = pixels ? _fbb.CreateVector<uint8_t>(*pixels) : 0;
	return dv::CreateFrame(_fbb, timestamp, timestampStartOfFrame, timestampEndOfFrame, timestampStartOfExposure,
		timestampEndOfExposure, format, sizeX, sizeY, positionX, positionY, pixels__);
}

flatbuffers::Offset<Frame> CreateFrame(flatbuffers::FlatBufferBuilder &_fbb, const FrameT *_o,
	const flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline FrameT *Frame::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
	auto _o = new FrameT();
	UnPackTo(_o, _resolver);
	return _o;
}

inline void Frame::UnPackTo(FrameT *_o, const flatbuffers::resolver_function_t *_resolver) const {
	(void) _o;
	(void) _resolver;
	UnPackToFrom(_o, this, _resolver);
}

inline void Frame::UnPackToFrom(FrameT *_o, const Frame *_fb, const flatbuffers::resolver_function_t *_resolver) {
	(void) _o;
	(void) _fb;
	(void) _resolver;
	{
		auto _e       = _fb->timestamp();
		_o->timestamp = _e;
	};
	{
		auto _e                   = _fb->timestampStartOfFrame();
		_o->timestampStartOfFrame = _e;
	};
	{
		auto _e                 = _fb->timestampEndOfFrame();
		_o->timestampEndOfFrame = _e;
	};
	{
		auto _e                      = _fb->timestampStartOfExposure();
		_o->timestampStartOfExposure = _e;
	};
	{
		auto _e                    = _fb->timestampEndOfExposure();
		_o->timestampEndOfExposure = _e;
	};
	{
		auto _e    = _fb->format();
		_o->format = _e;
	};
	{
		auto _e   = _fb->sizeX();
		_o->sizeX = _e;
	};
	{
		auto _e   = _fb->sizeY();
		_o->sizeY = _e;
	};
	{
		auto _e       = _fb->positionX();
		_o->positionX = _e;
	};
	{
		auto _e       = _fb->positionY();
		_o->positionY = _e;
	};
	{
		auto _e = _fb->pixels();
		if (_e) {
			_o->pixels.resize(_e->size());
			for (flatbuffers::uoffset_t _i = 0; _i < _e->size(); _i++) {
				_o->pixels[_i] = _e->Get(_i);
			}
		}
	};
}

inline flatbuffers::Offset<Frame> Frame::Pack(
	flatbuffers::FlatBufferBuilder &_fbb, const FrameT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	return CreateFrame(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<Frame> CreateFrame(
	flatbuffers::FlatBufferBuilder &_fbb, const FrameT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
	(void) _rehasher;
	(void) _o;
	struct _VectorArgs {
		flatbuffers::FlatBufferBuilder *__fbb;
		const FrameT *__o;
		const flatbuffers::rehasher_function_t *__rehasher;
	} _va = {&_fbb, _o, _rehasher};
	(void) _va;
	auto _timestamp                = _o->timestamp;
	auto _timestampStartOfFrame    = _o->timestampStartOfFrame;
	auto _timestampEndOfFrame      = _o->timestampEndOfFrame;
	auto _timestampStartOfExposure = _o->timestampStartOfExposure;
	auto _timestampEndOfExposure   = _o->timestampEndOfExposure;
	auto _format                   = _o->format;
	auto _sizeX                    = _o->sizeX;
	auto _sizeY                    = _o->sizeY;
	auto _positionX                = _o->positionX;
	auto _positionY                = _o->positionY;
	auto _pixels                   = _o->pixels.size() ? _fbb.CreateVector(_o->pixels.data(), _o->pixels.size()) : 0;
	return dv::CreateFrame(_fbb, _timestamp, _timestampStartOfFrame, _timestampEndOfFrame, _timestampStartOfExposure,
		_timestampEndOfExposure, _format, _sizeX, _sizeY, _positionX, _positionY, _pixels);
}

inline const flatbuffers::TypeTable *FrameFormatTypeTable() {
	static const flatbuffers::TypeCode type_codes[]
		= {{flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_CHAR, 0, 0}};
	static const flatbuffers::TypeFunction type_refs[] = {FrameFormatTypeTable};
	static const int64_t values[]                      = {0, 16, 24};
	static const char *const names[]                   = {"GRAY", "BGR", "BGRA"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_ENUM, 3, type_codes, type_refs, values, names};
	return &tt;
}

inline const flatbuffers::TypeTable *FrameTypeTable() {
	static const flatbuffers::TypeCode type_codes[]    = {{flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_LONG, 0, -1},
        {flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_LONG, 0, -1}, {flatbuffers::ET_LONG, 0, -1},
        {flatbuffers::ET_CHAR, 0, 0}, {flatbuffers::ET_SHORT, 0, -1}, {flatbuffers::ET_SHORT, 0, -1},
        {flatbuffers::ET_SHORT, 0, -1}, {flatbuffers::ET_SHORT, 0, -1}, {flatbuffers::ET_UCHAR, 1, -1}};
	static const flatbuffers::TypeFunction type_refs[] = {FrameFormatTypeTable};
	static const char *const names[]
		= {"timestamp", "timestampStartOfFrame", "timestampEndOfFrame", "timestampStartOfExposure",
			"timestampEndOfExposure", "format", "sizeX", "sizeY", "positionX", "positionY", "pixels"};
	static const flatbuffers::TypeTable tt = {flatbuffers::ST_TABLE, 11, type_codes, type_refs, nullptr, names};
	return &tt;
}

inline const dv::Frame *GetFrame(const void *buf) {
	return flatbuffers::GetRoot<dv::Frame>(buf);
}

inline const dv::Frame *GetSizePrefixedFrame(const void *buf) {
	return flatbuffers::GetSizePrefixedRoot<dv::Frame>(buf);
}

inline const char *FrameIdentifier() {
	return "FRME";
}

const char *Frame::identifier = FrameIdentifier();

inline bool FrameBufferHasIdentifier(const void *buf) {
	return flatbuffers::BufferHasIdentifier(buf, FrameIdentifier());
}

inline bool VerifyFrameBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifyBuffer<dv::Frame>(FrameIdentifier());
}

inline bool VerifySizePrefixedFrameBuffer(flatbuffers::Verifier &verifier) {
	return verifier.VerifySizePrefixedBuffer<dv::Frame>(FrameIdentifier());
}

inline void FinishFrameBuffer(flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<dv::Frame> root) {
	fbb.Finish(root, FrameIdentifier());
}

inline void FinishSizePrefixedFrameBuffer(flatbuffers::FlatBufferBuilder &fbb, flatbuffers::Offset<dv::Frame> root) {
	fbb.FinishSizePrefixed(root, FrameIdentifier());
}

inline std::unique_ptr<FrameT> UnPackFrame(const void *buf, const flatbuffers::resolver_function_t *res = nullptr) {
	return std::unique_ptr<FrameT>(GetFrame(buf)->UnPack(res));
}

} // namespace dv

#endif // FLATBUFFERS_GENERATED_FRAME_DV_H_
