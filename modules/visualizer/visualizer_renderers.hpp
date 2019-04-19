#ifndef VISUALIZER_RENDERERS_HPP_
#define VISUALIZER_RENDERERS_HPP_

#include "dv-sdk/data/event.hpp"
#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/data/imu.hpp"

#include "ext/sfml/helpers.hpp"
#include "ext/sfml/line.hpp"

class Renderer {
protected:
	int16_t renderSizeX;
	int16_t renderSizeY;
	sf::RenderWindow *renderWindow;
	sf::Font *renderFont;
	float renderZoomFactor;

public:
	Renderer(int16_t renderSizeX_, int16_t renderSizeY_, sf::RenderWindow *renderWindow_, sf::Font *renderFont_) :
		renderSizeX(renderSizeX_),
		renderSizeY(renderSizeY_),
		renderWindow(renderWindow_),
		renderFont(renderFont_),
		renderZoomFactor(1.0) {
	}

	void setZoomFactor(float zoom) {
		renderZoomFactor = zoom;
	}

	virtual ~Renderer() {
	}

	virtual void render(const void *wrapper) = 0;
};

class EventRenderer : public Renderer {
public:
	EventRenderer(int16_t renderSizeX_, int16_t renderSizeY_, sf::RenderWindow *renderWindow_, sf::Font *renderFont_) :
		Renderer(renderSizeX_, renderSizeY_, renderWindow_, renderFont_) {
	}

	void render(const void *wrapper) override {
		auto eventPacket   = static_cast<const dv::EventPacket::NativeTableType *>(wrapper);
		const auto &events = eventPacket->events;

		std::vector<sf::Vertex> vertices;
		vertices.reserve(events.size() * 4);

		// Render all valid events.
		for (const auto &evt : events) {
			// ON polarity (green), OFF polarity (red).
			sfml::Helpers::addPixelVertices(
				vertices, evt.x(), evt.y(), renderZoomFactor, (evt.polarity()) ? (sf::Color::Green) : (sf::Color::Red));
		}

		renderWindow->draw(vertices.data(), vertices.size(), sf::Quads);
	}
};

class FrameRenderer : public Renderer {
private:
	sf::Sprite sprite;
	sf::Texture texture;
	std::vector<uint8_t> pixels;

public:
	FrameRenderer(int16_t renderSizeX_, int16_t renderSizeY_, sf::RenderWindow *renderWindow_, sf::Font *renderFont_) :
		Renderer(renderSizeX_, renderSizeY_, renderWindow_, renderFont_) {
		// Create texture representing frame, set smoothing.
		texture.create(static_cast<unsigned int>(renderSizeX), static_cast<unsigned int>(renderSizeY));
		texture.setSmooth(false);

		// Assign texture to sprite.
		sprite.setTexture(texture);

		// 32-bit RGBA pixels (8-bit per channel), standard CG layout.
		pixels.reserve(static_cast<size_t>(renderSizeX * renderSizeY * 4));
	}

	void render(const void *wrapper) override {
		auto frame = static_cast<const dv::Frame::NativeTableType *>(wrapper);

		// 32-bit RGBA pixels (8-bit per channel), standard CG layout.
		switch (frame->format) {
			case dv::FrameFormat::GRAY: {
				for (size_t srcIdx = 0, dstIdx = 0; srcIdx < frame->pixels.size();) {
					uint8_t greyValue = frame->pixels[srcIdx];
					pixels[dstIdx++]  = greyValue; // R
					pixels[dstIdx++]  = greyValue; // G
					pixels[dstIdx++]  = greyValue; // B
					pixels[dstIdx++]  = UINT8_MAX; // A
					srcIdx++;
				}
				break;
			}

			case dv::FrameFormat::BGR: {
				for (size_t srcIdx = 0, dstIdx = 0; srcIdx < frame->pixels.size();) {
					pixels[dstIdx++] = frame->pixels[srcIdx + 2]; // R
					pixels[dstIdx++] = frame->pixels[srcIdx + 1]; // G
					pixels[dstIdx++] = frame->pixels[srcIdx + 0]; // B
					pixels[dstIdx++] = UINT8_MAX;                 // A
					srcIdx += 3;
				}
				break;
			}

			case dv::FrameFormat::BGRA: {
				for (size_t srcIdx = 0, dstIdx = 0; srcIdx < frame->pixels.size();) {
					pixels[dstIdx++] = frame->pixels[srcIdx + 2]; // R
					pixels[dstIdx++] = frame->pixels[srcIdx + 1]; // G
					pixels[dstIdx++] = frame->pixels[srcIdx + 0]; // B
					pixels[dstIdx++] = frame->pixels[srcIdx + 3]; // A
					srcIdx += 4;
				}
				break;
			}
		}

		texture.update(pixels.data(), static_cast<unsigned int>(frame->sizeX), static_cast<unsigned int>(frame->sizeY),
			static_cast<unsigned int>(frame->positionX), static_cast<unsigned int>(frame->positionY));

		sprite.setTextureRect(sf::IntRect(frame->positionX, frame->positionY, frame->sizeX, frame->sizeY));

		sprite.setPosition(static_cast<float>(frame->positionX) * renderZoomFactor,
			static_cast<float>(frame->positionY * renderZoomFactor));

		sprite.setScale(renderZoomFactor, renderZoomFactor);

		renderWindow->draw(sprite);
	}
};

#define RESET_LIMIT_POS(VAL, LIMIT) \
	if ((VAL) > (LIMIT)) {          \
		(VAL) = (LIMIT);            \
	}
#define RESET_LIMIT_NEG(VAL, LIMIT) \
	if ((VAL) < (LIMIT)) {          \
		(VAL) = (LIMIT);            \
	}

class IMURenderer : public Renderer {
public:
	IMURenderer(int16_t renderSizeX_, int16_t renderSizeY_, sf::RenderWindow *renderWindow_, sf::Font *renderFont_) :
		Renderer(renderSizeX_, renderSizeY_, renderWindow_, renderFont_) {
	}

	void render(const void *wrapper) override {
		auto imuPacket         = static_cast<const dv::IMUPacket::NativeTableType *>(wrapper);
		const auto &imuSamples = imuPacket->samples;

		float scaleFactorAccel = 30 * renderZoomFactor;
		float scaleFactorGyro  = 15 * renderZoomFactor;
		float lineThickness    = 4 * renderZoomFactor;
		float maxSizeX         = static_cast<float>(renderSizeX) * renderZoomFactor;
		float maxSizeY         = static_cast<float>(renderSizeY) * renderZoomFactor;

		sf::Color accelColor = sf::Color::Green;
		sf::Color gyroColor  = sf::Color::Magenta;

		float centerPointX = maxSizeX / 2;
		float centerPointY = maxSizeY / 2;

		float accelX = 0, accelY = 0, accelZ = 0;
		float gyroX = 0, gyroY = 0, gyroZ = 0;
		float temp = 0;

		// Iterate over valid IMU events and average them.
		// This somewhat smoothes out the rendering.
		for (const auto &imu : imuSamples) {
			accelX += imu.accelerometerX;
			accelY += imu.accelerometerY;
			accelZ += imu.accelerometerZ;

			gyroX += imu.gyroscopeX;
			gyroY += imu.gyroscopeY;
			gyroZ += imu.gyroscopeZ;

			temp += imu.temperature;
		}

		// Normalize values.
		accelX /= static_cast<float>(imuSamples.size());
		accelY /= static_cast<float>(imuSamples.size());
		accelZ /= static_cast<float>(imuSamples.size());

		gyroX /= static_cast<float>(imuSamples.size());
		gyroY /= static_cast<float>(imuSamples.size());
		gyroZ /= static_cast<float>(imuSamples.size());

		temp /= static_cast<float>(imuSamples.size());

		// Acceleration X, Y as lines. Z as a circle.
		float accelXScaled = centerPointX - accelX * scaleFactorAccel;
		RESET_LIMIT_POS(accelXScaled, maxSizeX - 2 - lineThickness)
		RESET_LIMIT_NEG(accelXScaled, 1 + lineThickness)
		float accelYScaled = centerPointY - accelY * scaleFactorAccel;
		RESET_LIMIT_POS(accelYScaled, maxSizeY - 2 - lineThickness)
		RESET_LIMIT_NEG(accelYScaled, 1 + lineThickness)
		float accelZScaled = std::fabs(accelZ * scaleFactorAccel);
		RESET_LIMIT_POS(accelZScaled, centerPointY - 2 - lineThickness) // Circle max.
		RESET_LIMIT_NEG(accelZScaled, 1)                                // Circle min.

		sfml::Line accelLine(sf::Vector2f(centerPointX, centerPointY), sf::Vector2f(accelXScaled, accelYScaled),
			lineThickness, accelColor);
		renderWindow->draw(accelLine);

		sf::CircleShape accelCircle(accelZScaled);
		sfml::Helpers::setOriginToCenter(accelCircle);
		accelCircle.setFillColor(sf::Color::Transparent);
		accelCircle.setOutlineColor(accelColor);
		accelCircle.setOutlineThickness(-lineThickness);
		accelCircle.setPosition(sf::Vector2f(centerPointX, centerPointY));

		renderWindow->draw(accelCircle);

		// Gyroscope pitch(X), yaw(Y), roll(Z) as lines.
		float gyroXScaled = centerPointY + gyroX * scaleFactorGyro;
		RESET_LIMIT_POS(gyroXScaled, maxSizeY - 2 - lineThickness)
		RESET_LIMIT_NEG(gyroXScaled, 1 + lineThickness)
		float gyroYScaled = centerPointX + gyroY * scaleFactorGyro;
		RESET_LIMIT_POS(gyroYScaled, maxSizeX - 2 - lineThickness)
		RESET_LIMIT_NEG(gyroYScaled, 1 + lineThickness)
		float gyroZScaled = centerPointX - gyroZ * scaleFactorGyro;
		RESET_LIMIT_POS(gyroZScaled, maxSizeX - 2 - lineThickness)
		RESET_LIMIT_NEG(gyroZScaled, 1 + lineThickness)

		sfml::Line gyroLine1(
			sf::Vector2f(centerPointX, centerPointY), sf::Vector2f(gyroYScaled, gyroXScaled), lineThickness, gyroColor);
		renderWindow->draw(gyroLine1);

		sfml::Line gyroLine2(sf::Vector2f(centerPointX, centerPointY - 20),
			sf::Vector2f(gyroZScaled, centerPointY - 20), lineThickness, gyroColor);
		renderWindow->draw(gyroLine2);

		// TODO: enhance IMU renderer with more text info.
		char valStr[128];

		// Acceleration X/Y.
		snprintf(valStr, 128, "%.2f,%.2f g", static_cast<double>(accelX), static_cast<double>(accelY));

		sf::Text accelText(valStr, *renderFont, 30);
		sfml::Helpers::setTextColor(accelText, accelColor);
		accelText.setPosition(sf::Vector2f(accelXScaled, accelYScaled));

		renderWindow->draw(accelText);

		// Acceleration Z.
		snprintf(valStr, 128, "%.2f g", static_cast<double>(accelZ));

		sf::Text accelTextZ(valStr, *renderFont, 30);
		sfml::Helpers::setTextColor(accelTextZ, accelColor);
		accelTextZ.setPosition(sf::Vector2f(centerPointX, centerPointY + accelZScaled + lineThickness));

		renderWindow->draw(accelTextZ);

		// Temperature.
		snprintf(valStr, 128, "Temp: %.2f C", static_cast<double>(temp));

		sf::Text tempText(valStr, *renderFont, 30);
		sfml::Helpers::setTextColor(tempText, sf::Color::White);
		tempText.setPosition(sf::Vector2f(0, 0));

		renderWindow->draw(tempText);
	}
};

#endif // VISUALIZER_RENDERERS_HPP_
