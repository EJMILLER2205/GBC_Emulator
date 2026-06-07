#include "ppu.h"
#include <stdexcept>
#include <iostream>

PPU::PPU(Bus& bus) : bus(&bus) {}

void PPU::tick(int cycles) {
	if (!(getLCDC() & 0x80)) {
		// LCD off — still need to count cycles for timing
		cycleCount += cycles;
		if (cycleCount >= 70224) {
			cycleCount -= 70224;
			// Fire VBlank interrupt even with LCD off
			// Some games need this to proceed with initialization
			uint8_t IF = bus->read(0xFF0F);
			bus->write(0xFF0F, IF | 0x01);
			frameComplete = true;
		}
		setLY(0);
		setMode(0);
		return;
	}
	// Check if LCD is enabled
	if (!(getLCDC() & 0x80)) {
		return;
	}

	cycleCount += cycles;

	uint8_t ly = getLY();

	if (ly < 144) {
		// Visible scanlines
		if (cycleCount >= 80 && cycleCount < 252) {
			// Mode 3 - Drawing
			setMode(3);
		}
		else if (cycleCount >= 252 && cycleCount < 456) {
			// Mode 0 - HBlank
			setMode(0);
		}
		else if (cycleCount < 80) {
			// Mode 2 - OAM scan
			setMode(2);
		}
	}
	else {
		// VBlank
		setMode(1);
	}

	// End of scanline
	if (cycleCount >= 456) {
		cycleCount -= 456;
		renderScanline(); // Render this line before advancing
		setLY(ly + 1);

		// LYC check
		uint8_t newLY = getLY();
		uint8_t lyc = getLYC();
		uint8_t stat = bus->read(0xFF41);

		if (newLY == lyc) {
			// Set LYC=LY flag (bit 2)
			stat |= 0x04;
			bus->write(0xFF41, stat);
			// Fire STAT interrupt if LYC interrupt enabled (bit 6)
			if (stat & 0x40) requestSTAT();
		}
		else {
			// Clear LYC=LY flag
			stat &= ~0x04;
			bus->write(0xFF41, stat);
		}

		// End of frame (all 144 visible and 10 VBlank scanlines)
		if (getLY() == 154) {
			setLY(0);
			frameComplete = true;
			windowLine = 0; // reset window line counter
			bgColorIndex.fill(0); // reset color index buffer
		}

		// Fire VBlank interrupt at line 144
		if (getLY() == 144) {
			uint8_t IF = bus->read(0xFF0F);
			bus->write(0xFF0F, IF | 0x01); // set bit 1
		}
	}
}

bool PPU::frameReady() { 
	return frameComplete; 
}

void PPU::clearFrameReady() { 
	frameComplete = false; 
}

void PPU::renderScanline() {
	uint8_t lcdc = getLCDC();
	uint8_t ly = getLY();

	// Only render visible scanlines
	if (ly >= 144) return;

	// Only render if LCD is on
	if (!(lcdc & 0x80)) return;

	// Render background if enabled (LCDC bit 0)
	if (lcdc & 0x01) {
		renderBackground();
	}

	// Render window if enabled (LCDC bit 5)
	if (lcdc & 0x20) {
		renderWindow();
	}

	// Render sprites
	renderSprites();
}

void PPU::renderBackground() {
	uint8_t ly = getLY();
	if (ly >= 144) return; // Safety check

	uint8_t lcdc = getLCDC();
	uint8_t scy = getSCY();
	uint8_t scx = getSCX();
	uint8_t bgp = getBGP();

	// Chooses tile map (LCDC bit 3)
	uint16_t tileMapBase = (lcdc & 0x08) ? 0x9C00 : 0x9800; // 0 = 0x9800, 1 = 0x9C00

	// Chooses which tile data region (LCDC bit 4)
	bool unsignedIndex = (lcdc & 0x10); // 0 = 0x8800 (signed indexing, tile 0 is at 0x9000), 1 = 0x8000 (unsigned indexing)

	// Chooses which row of the background it is on (accounting for scroll)
	uint8_t bgY = ly + scy;       // Wraps naturally since both are using uint8_t
	uint8_t tileRow = bgY / 8;    // Which row of tiles
	uint8_t tilePixelY = bgY % 8; // Which pixel row inside the tile

	// Render all 160 pixels on this scanline
	for (int px = 0; px < 160; px++) {
		uint8_t bgX = px + scx;       // wraps natrually
		uint8_t tileCol = bgX / 8;    // which column of tiles
		uint8_t tilePixelX = bgX % 8; // which pixel column within the tile

		// Look up tile index from tile map
		uint16_t tileMapAddr = tileMapBase + (tileRow * 32) + tileCol;
		uint8_t tileIndex = bus->read(tileMapAddr);

		// Get tile data address
		uint16_t tileDataAddr;
		if (unsignedIndex) {
			tileDataAddr = 0x8000 + (tileIndex * 16); // 0x8000 base, tile index is unsigned 0-255
		}
		else {
			// 0x8000 base, tile index is signed -128 to 127
			// tile 0 lives at 0x9000
			int8_t signedIndex = (int8_t)(tileIndex);
			tileDataAddr = 0x9000 + (signedIndex * 16);
		}

		// Get the two bytes for this pixel row within the tile
		uint8_t byte1 = bus->read(tileDataAddr + (tilePixelY * 2));
		uint8_t byte2 = bus->read(tileDataAddr + (tilePixelY * 2) + 1);

		// Extract the 2-bit color index for this pixel
		// bit 7 is leftmost pixel, bit 0 is rightmost
		uint8_t bitPos = 7 - tilePixelX;
		uint8_t colorBit_lo = (byte1 >> bitPos) & 1;
		uint8_t colorBit_hi = (byte2 >> bitPos) & 1;
		uint8_t colorIndex = (colorBit_hi << 1) | colorBit_lo;

		// Apply BGP palette
		// BGP maps color indices to shades
		// bits 1-0 = shade for index 0
		// bits 3-2 = shade for index 1
		// bits 5-4 = shade for index 2
		// bits 7-6 = shade for index 3
		uint8_t shade = (bgp >> (colorIndex * 2)) & 0x03;

		// Map shade to RGB color
		static const uint32_t colors[4] = {
			0xE0F8D0, // 0 = white
			0x88C070, // 1 = light gray
			0x346856, // 2 = dark gray
			0x081820  // 3 = black
		};
		bgColorIndex[ly * 160 + px] = colorIndex; // store before palette lookup
		// Puts the pixel shade into the frame buffer
		framebuffer[ly * 160 + px] = colors[shade];
	}
}

void PPU::renderSprites() {
	uint8_t bgp = getBGP();
	uint8_t ly = getLY();
	if (ly >= 144) return;

	uint8_t lcdc = getLCDC();
	// Check if sprites are enabled
	if (!(lcdc & 0x02)) return;

	// Sprite height - 8 or 16 depending on LCDC bit 2
	int spriteHeight = (lcdc & 0x04) ? 16 : 8;

	// Collect visible sprites for current scanline (max of 10)
	struct Sprite {
		int y, x, tile, flags;
	};
	Sprite visible[10];
	int count = 0;

	// Loop through all 40 sprites in OAM
	for (int i = 0; i < 40 && count < 10; i++) {
		int oamAddr = 0xFE00 + (i * 4);
		int spriteY = bus->read(oamAddr + 0) - 16; // Actual screen Y
		int spriteX = bus->read(oamAddr + 1) - 8;  // Actual screen X
		int tile = bus->read(oamAddr + 2);
		int flags = bus->read(oamAddr + 3);

		//Check if the sprite falls on the current scanline
		if (ly >= spriteY && ly < spriteY + spriteHeight) {
			visible[count++] = {
				spriteY,
				spriteX,
				tile,
				flags
			};
		}
	}

	// Draw all visible sprites
	for (int i = 0; i < count; i++) {
		int spriteRow;

		// Calculate which row of the sprite it's on
		// Checks for y-flip
		if ((visible[i].flags) & 0x40) {
			spriteRow = (spriteHeight - 1) - (ly - visible[i].y);
		}
		else {
			spriteRow = ly - visible[i].y;
		}

		// For 8x16 sprites, bit 0 of tile index is ignored
		int tile = visible[i].tile;
		if (spriteHeight == 16) tile &= 0xFE;

		// Gets the sprite bytes for the row
		uint16_t tileAddr = 0x8000 + (tile * 16) + (spriteRow * 2);
		uint8_t byte1 = bus->read(tileAddr);
		uint8_t byte2 = bus->read(tileAddr + 1);

		// Get all colors for the each pixel
		for (int px = 0; px < 8; px++) {
			uint8_t bitPos = 7 - px;
			// Check for x-flip
			if (visible[i].flags & 0x20) bitPos = px;
			// Get colors
			uint8_t colorBit_lo = (byte1 >> bitPos) & 1;
			uint8_t colorBit_hi = (byte2 >> bitPos) & 1;
			uint8_t colorIndex = (colorBit_hi << 1) | colorBit_lo;
			if (colorIndex == 0) continue;
			uint8_t pallete = (visible[i].flags & 0x10) ? getOBP1() : getOBP0();
			uint8_t shade = (pallete >> (colorIndex * 2)) & 0x03;
			// Colors struct
			static const uint32_t colors[4] = {
				0xE0F8D0, // 0 = white
				0x88C070, // 1 = light gray
				0x346856, // 2 = dark gray
				0x081820  // 3 = black
			};
			// Write colors
			int screenX = visible[i].x + px;
			if (screenX < 0 || screenX >= 160) continue;

			// Check sprite priority (flag bit 7)
			// If set, sprite renders behind background colors 1-3
			if (visible[i].flags & 0x80) {
				if (bgColorIndex[ly * 160 + screenX] != 0) continue;
			}
			framebuffer[ly * 160 + screenX] = colors[shade];
		}
	}
}

void PPU::setMode(uint8_t mode) {
	uint8_t stat = bus->read(0xFF41);
	uint8_t currentMode = stat & 0x03;

	// Only act on mode transitions
	if (currentMode == mode) return;

	//Update mode bits
	stat = (stat & 0xFC) | (mode & 0x03);
	bus->write(0xFF41, stat);

	// Fire STAT interrupt based on new mode
	switch (mode) {
	case 0: if (stat & 0x08) requestSTAT(); break; // HBlank
	case 1: if (stat & 0x10) requestSTAT(); break; // VBlank
	case 2: if (stat & 0x20) requestSTAT(); break; // OAM scan
	}
}

void PPU::renderWindow() {
	uint8_t ly = getLY();
	if (ly >= 144) return;
	uint8_t lcdc = getLCDC();

	// Check if window is enabled
	if (!(lcdc & 0x20)) return;

	uint8_t wy = getWY();
	uint8_t wx = getWX();

	// Window only draws on scanlines at or below WY
	if (ly < wy) return;

	// Actual screen X where window starts (WX is offset by 7)
	int windowX = (int)wx - 7;

	// Which tile map the window uses - LCDC bit 6
	// 0 = 0x9800, 1 = 0x9C00
	uint16_t tileMapBase = (lcdc & 0x40) ? 0x9C00 : 0x9800;

	// Which tile data region - LCDC bit 4
	bool unsignedIndex = (lcdc & 0x10);

	// Window has it's own internal line counter
	uint8_t winY = windowLine;
	uint8_t tileRow = winY / 8;
	uint8_t tilePixelY = winY % 8;

	// Render pixels from windowX to 160
	for (int px = windowX; px < 160; px++) {
		if (px < 0) continue;  // If window starts offscreen

		int winPx = px - windowX; // Pixel position within window
		uint8_t tileCol = winPx / 8;
		uint8_t tilePixelX = winPx % 8;

		// Look up tile index from tile map
		uint16_t tileMapAddr = tileMapBase + (tileRow * 32) + tileCol;
		uint8_t tileIndex = bus->read(tileMapAddr);

		// Get tile data address
		uint16_t tileDataAddr;
		if (unsignedIndex) {
			tileDataAddr = 0x8000 + (tileIndex * 16);
		}
		else {
			int8_t signedIndex = (int8_t)(tileIndex);
			tileDataAddr = 0x9000 + (signedIndex * 16);
		}

		// Get two bytes for this pixel row
		uint8_t byte1 = bus->read(tileDataAddr + (tilePixelY * 2));
		uint8_t byte2 = bus->read(tileDataAddr + (tilePixelY * 2) + 1);

		// Extract 2 bit color index
		uint8_t bitPos = 7 - tilePixelX;
		uint8_t colorBit_lo = (byte1 >> bitPos) & 1;
		uint8_t colorBit_hi = (byte2 >> bitPos) & 1;
		uint8_t colorIndex = (colorBit_hi << 1) | colorBit_lo;

		// Apply BGP palette
		uint8_t bgp = getBGP();
		uint8_t shade = (bgp >> (colorIndex * 2)) & 0x03;

		static const uint32_t colors[4] = {
			0xE0F8D0, // white
			0x88C070, // light gray
			0x346856, // dark gray
			0x081820  // black
		};
		bgColorIndex[ly * 160 + px] = colorIndex;
		framebuffer[ly * 160 + px] = colors[shade];
	}
	windowLine++;
}
