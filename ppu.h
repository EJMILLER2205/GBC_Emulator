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
	int windowLine = 0; // tracks which window line we're on
	bool frameComplete = false;

	// Helper functions
	void renderScanline(); // Renders current LY scanline
	void renderBackground(); // Draws background tiles
	void renderSprites(); // Draws sprites
	void renderWindow(); // Renders window
	bool scanlineRendered = false; // Track if we have rendered the scanline
	uint8_t getLY() { return bus->read(0xFF44); }
	void setLY(uint8_t v) { bus->write(0xFF44, v); }
	uint8_t getLCDC() { return bus->read(0xFF40); }
	uint8_t getSCY() { return bus->read(0xFF42); }
	uint8_t getSCX() { return bus->read(0xFF43); }
	uint8_t getLYC() { return bus->read(0xFF45); }
	uint8_t getBGP() { return bus->read(0xFF47); }
	void setMode(uint8_t mode);
	void requestSTAT() {
		uint8_t IF = bus->read(0xFF0F);
		bus->write(0xFF0F, IF | 0x02); // set bit 1 of IF
	}

	// Sprite reading helper functions
	uint8_t getOBP0() { return bus->read(0xFF48); }
	uint8_t getOBP1() { return bus->read(0xFF49); }

	// Window reading helper functions
	uint8_t getWY() { return bus->read(0xFF4A); }
	uint8_t getWX() { return bus->read(0xFF4B); }

	std::array<uint8_t, 160 * 144> bgColorIndex{}; // tracks bg color index per pixel

	bool vblankPending = false;

	uint8_t latchedSCX = 0;
	uint8_t latchedSCY = 0;
	bool scrollLatched = false;
};
