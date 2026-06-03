#pragma once
#include <cstdint>
#include <array>
#include "bus.h"

class PPU {
public:
	PPU(Bus& bus);
	void tick(int cycles); // Called every cpu step
	bool frameReady();     // True when full frame is done
	void clearFrameReady();
	std::array<uint32_t, 160 * 144> framebuffer{}; // 160x144 framebuffer, each pixel is RGB packed into uint32_t

private:
	Bus* bus;
	int cycleCount = 0; // Cycles accumulated on current scanline
	bool frameComplete = false;

	// Helper functions
	void renderScanline(); // Renders current LY scanline
	void renderBackground(); // Draws background tiles
	void renderSprites(); // Draws sprites
	uint8_t getLY() { return bus->read(0xFF44); }
	void setLY(uint8_t v) { bus->write(0xFF44, v); }
	uint8_t getLCDC() { return bus->read(0xFF40); }
	uint8_t getSCY() { return bus->read(0xFF42); }
	uint8_t getSCX() { return bus->read(0xFF43); }
	uint8_t getBGP() { return bus->read(0xFF47); }
	void setMode(uint8_t mode);
};