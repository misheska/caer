#ifndef DV_OUTPUT_HPP
#define DV_OUTPUT_HPP

#include "dv-sdk/events/frame8.hpp"
#include "dv-sdk/events/polarity.hpp"

#include "dv-sdk/utils.h"

#include <fstream>

using PackFuncPtr = flatbuffers::Offset<void> (*)(
	flatbuffers::FlatBufferBuilder &, const void *, const flatbuffers::rehasher_function_t *);
using UnpackFuncPtr = void *(*) (const flatbuffers::resolver_function_t *);

struct dvType {
	std::string identifier;
	PackFuncPtr pack;
	UnpackFuncPtr unpack;
};

dvType supportedTypes[2] = {
	{"POLA", &PolarityPacket::Pack, &PolarityPacket::UnPack},
	{"FRM8", &Frame8Packet::Pack, &Frame8Packet::UnPack},
};

enum class OutputTypes {
	NETWORK_TCP,
	NETWORK_UDP,
	LOCAL_PIPE,
	LOCAL_FILE,
};

class dvOutput {
private:
	int fileIO;
	/// Track first packet container's lowest event timestamp that was sent out.
	int64_t firstTimestamp;
	/// Track last packet container's highest event timestamp that was sent out.
	int64_t lastTimestamp;
	/// The file for file writing.
	std::ofstream file;
	/// Output module statistics collection.
	struct output_common_statistics {
		uint64_t packetsNumber;
		uint64_t dataWritten;
	};

public:
	dvOutput(OutputTypes type);
};

#endif // DV_OUTPUT_HPP
