#ifndef DV_OUTPUT_HPP
#define DV_OUTPUT_HPP

#include "dv-sdk/events/frame8.hpp"
#include "dv-sdk/events/polarity.hpp"

#include "dv-sdk/utils.h"

struct arraydef {
	uint32_t typeId;
	void *ptr;
	size_t size;
};

struct dvOutputStatistics {
	uint64_t packetsNumber;
	uint64_t packetsSize;
	uint64_t dataWritten;

	dvOutputStatistics() : packetsNumber(0), packetsSize(0), dataWritten(0) {
	}
};

class dvOutput {
private:
	/// FlatBuffer builder.
	flatbuffers::FlatBufferBuilder builder;
	/// Apply compression.
	bool compression;
	/// Compression type flags.
	uint32_t compressionFlags;
	/// Output module statistics collection.
	struct dvOutputStatistics;

public:
	dvOutput() : builder(16 * 1024), compression(false), compressionFlags(0) {
	}

	void setCompression(bool compress) {
		compression = compress;
	}

	bool getCompression() {
		return (compression);
	}

	void setCompressionFlags(uint32_t compressFlags) {
		compressionFlags = compressFlags;
	}

	uint32_t getCompressionFlags() {
		return (compressionFlags);
	}

	struct arraydef processPacket(struct arraydef packet) {
		// Construct serialized flatbuffer packet.
		builder.Clear();
	}
};

#endif // DV_OUTPUT_HPP
