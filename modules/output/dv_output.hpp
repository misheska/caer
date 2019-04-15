#ifndef DV_OUTPUT_HPP
#define DV_OUTPUT_HPP

#include "dv-sdk/data/types.hpp"
#include "dv-sdk/utils.h"

struct dvOutputStatistics {
	uint64_t packetsNumber;
	uint64_t packetsSize;
	uint64_t dataWritten;

	dvOutputStatistics() : packetsNumber(0), packetsSize(0), dataWritten(0) {
	}
};

class dvOutput {
private:
	/// Apply compression.
	bool compression;
	/// Compression type flags.
	uint32_t compressionFlags;
	/// Output module statistics collection.
	struct dvOutputStatistics;

public:
	dvOutput() : compression(false), compressionFlags(0) {
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

	std::shared_ptr<const flatbuffers::FlatBufferBuilder> processPacket(const dv::Types::TypedObject *packet) {
		const auto typeInfo = dvTypeSystemGetInfoByID(packet->typeId);

		// Construct serialized flatbuffer packet.
		auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(16 * 1024);

		auto offset = (*typeInfo.pack)(msgBuild.get(), packet->obj);

		msgBuild->FinishSizePrefixed(flatbuffers::Offset<void>(offset), typeInfo.identifier);

		uint8_t *data   = msgBuild->GetBufferPointer();
		size_t dataSize = msgBuild->GetSize();

		if (compression) {
			// TODO: compression.
		}

		return (msgBuild);
	}
};

#endif // DV_OUTPUT_HPP
