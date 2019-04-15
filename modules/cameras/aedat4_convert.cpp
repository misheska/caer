#include "aedat4_convert.h"

#include <libcaercpp/events/frame.hpp>
#include <libcaercpp/events/imu6.hpp>
#include <libcaercpp/events/polarity.hpp>
#include <libcaercpp/events/special.hpp>

#include "dv-sdk/data/event.hpp"
#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/data/imu.hpp"
#include "dv-sdk/data/trigger.hpp"

void dvConvertToAedat4(caerEventPacketHeaderConst oldPacket, dvModuleData moduleData) {
	if (oldPacket == nullptr || moduleData == nullptr) {
		return;
	}

	if (caerEventPacketHeaderGetEventValid(oldPacket) <= 0) {
		// No valid events, nothing to do.
		return;
	}

	switch (caerEventPacketHeaderGetEventType(oldPacket)) {
		case POLARITY_EVENT: {
			auto newObject      = dvModuleOutputAllocate(moduleData, "events");
			auto newEventPacket = static_cast<dv::EventPacket::NativeTableType *>(newObject->obj);

			const libcaer::events::PolarityEventPacket oldPacketPolarity(
				const_cast<caerEventPacketHeader>(oldPacket), false);

			for (const auto &evt : oldPacketPolarity) {
				if (!evt.isValid()) {
					continue;
				}

				newEventPacket->events.emplace_back(
					evt.getTimestamp64(oldPacketPolarity), evt.getX(), evt.getY(), evt.getPolarity());
			}

			dvModuleOutputCommit(moduleData, "events");

			break;
		}

		case FRAME_EVENT: {
			const libcaer::events::FrameEventPacket oldPacketFrame(const_cast<caerEventPacketHeader>(oldPacket), false);

			for (const auto &evt : oldPacketFrame) {
				if (!evt.isValid()) {
					continue;
				}

				auto newObject = dvModuleOutputAllocate(moduleData, "frames");
				auto newFrame  = static_cast<dv::Frame::NativeTableType *>(newObject->obj);

				newFrame->timestamp                = evt.getTimestamp64(oldPacketFrame);
				newFrame->timestampStartOfFrame    = evt.getTSStartOfFrame64(oldPacketFrame);
				newFrame->timestampStartOfExposure = evt.getTSStartOfExposure64(oldPacketFrame);
				newFrame->timestampEndOfExposure   = evt.getTSEndOfExposure64(oldPacketFrame);
				newFrame->timestampEndOfFrame      = evt.getTSEndOfFrame64(oldPacketFrame);

				newFrame->sizeX     = static_cast<int16_t>(evt.getLengthX());
				newFrame->sizeY     = static_cast<int16_t>(evt.getLengthY());
				newFrame->positionX = static_cast<int16_t>(evt.getPositionX());
				newFrame->positionY = static_cast<int16_t>(evt.getPositionY());

				// New frame format specification.
				if (evt.getChannelNumber() == libcaer::events::FrameEvent::colorChannels::RGB) {
					// RGB to BGR.
					newFrame->format = dv::FrameFormat::BGR;
				}
				else if (evt.getChannelNumber() == libcaer::events::FrameEvent::colorChannels::RGBA) {
					// RGBA to BGRA.
					newFrame->format = dv::FrameFormat::BGRA;
				}
				else {
					// Default: grayscale.
					newFrame->format = dv::FrameFormat::GRAY;
				}

				newFrame->pixels.resize(evt.getPixelsMaxIndex());

				for (size_t px = 0; px < evt.getPixelsMaxIndex();) {
					switch (newFrame->format) {
						case dv::FrameFormat::GRAY:
							newFrame->pixels[px] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px] >> 8);
							px += 1;
							break;

						case dv::FrameFormat::BGR:
							newFrame->pixels[px + 0] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px + 2] >> 8);
							newFrame->pixels[px + 1] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px + 1] >> 8);
							newFrame->pixels[px + 2] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px + 0] >> 8);
							px += 3;
							break;

						case dv::FrameFormat::BGRA:
							newFrame->pixels[px + 0] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px + 2] >> 8);
							newFrame->pixels[px + 1] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px + 1] >> 8);
							newFrame->pixels[px + 2] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px + 0] >> 8);
							newFrame->pixels[px + 3] = static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px + 3] >> 8);
							px += 4;
							break;
					}
				}

				dvModuleOutputCommit(moduleData, "frames");
			}

			break;
		}

		case IMU6_EVENT: {
			auto newObject    = dvModuleOutputAllocate(moduleData, "imu");
			auto newIMUPacket = static_cast<dv::IMUPacket::NativeTableType *>(newObject->obj);

			const libcaer::events::IMU6EventPacket oldPacketIMU(const_cast<caerEventPacketHeader>(oldPacket), false);

			for (const auto &evt : oldPacketIMU) {
				if (!evt.isValid()) {
					continue;
				}

				newIMUPacket->samples.emplace_back();
			}

			dvModuleOutputCommit(moduleData, "imu");

			break;
		}

		case SPECIAL_EVENT: {
			auto newObject      = dvModuleOutputAllocate(moduleData, "events");
			auto newEventPacket = static_cast<dv::EventPacket::NativeTableType *>(newObject->obj);

			const libcaer::events::PolarityEventPacket oldPacketPolarity(
				const_cast<caerEventPacketHeader>(oldPacket), false);

			for (const auto &evt : oldPacketPolarity) {
				if (!evt.isValid()) {
					continue;
				}

				newEventPacket->events.emplace_back(
					evt.getTimestamp64(oldPacketPolarity), evt.getX(), evt.getY(), evt.getPolarity());
			}

			dvModuleOutputCommit(moduleData, "events");

			break;
		}

		default:
			// Unknown data.
			break;
	}
}
