#include "bus.h"
#include <fstream>
#include <iostream>
#include "joypad.h"

bool Bus::loadROM(const std::string& path) {
	// Opens file for reading (raw bytes | start cursor at end of file)
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) { // Makes sure file has opened properly
		std::cerr << "Failed to open ROM\n"; 
		return false; 
	}
	size_t size = f.tellg(); // Gets current size by returning position of cursor (at end of file)
	f.seekg(0); // Set cursor at beginning of file (seek get position 0)
	rom.resize(size); // Sizes the rom vector to the size of the file
	// Reading all bytes of data from file f and putting it into rom.data() for a size (size) and interpretting the uint8_t as a char for read command
	f.read(reinterpret_cast<char*>(rom.data()), size); // Also more efficient than a long for loop, as this can copy chunks of data more efficiently
	// Detect MBC type from cartridge header byte 0x0147
	if (rom.size() > 0x0147) {
		uint8_t cartType = rom[0x0147];
		hasMBC1 = (cartType >= 0x01 && cartType <= 0x03);
		std::cerr << "Cart type: 0x" << std::hex << (int)cartType << "\n";
	}
	return true;
}

uint8_t Bus::read(uint16_t addr) {
	// ROM bank 0 — always fixed
	if (addr <= 0x3FFF) return addr < rom.size() ? rom[addr] : 0xFF;

	// ROM bank 1+ — switchable
	if (addr <= 0x7FFF) {
		uint32_t bankAddr = (romBank * 0x4000) + (addr - 0x4000);
		return bankAddr < rom.size() ? rom[bankAddr] : 0xFF;
	}
	if (addr <= 0x9FFF) return vram[addr - 0x8000];
	if (addr <= 0xBFFF) return 0xFF;
	if (addr <= 0xCFFF) return wram[addr - 0xC000];
	if (addr <= 0xDFFF) return wram[addr - 0xD000 + 0x1000]; // Will implement bank checking later
	if (addr <= 0xFDFF) return 0xFF;
	if (addr <= 0xFE9F) return oam[addr - 0xFE00];
	if (addr <= 0xFEFF) return 0xFF;
	if (addr == 0xFF00) return joypad ? joypad->read() : 0xFF; // If joypad is read then return the joypad values if joypad is enabled
	if (addr <= 0xFF7F) return io[addr - 0xFF00];
	if (addr <= 0xFFFE) return hram[addr - 0xFF80];
	return ie;
}

void Bus::write(uint16_t addr, uint8_t val) {
	// MBC1 bank switching — game writes to ROM address space to switch banks
	if (addr >= 0x2000 && addr <= 0x3FFF && hasMBC1) {
		romBank = val & 0x1F; // lower 5 bits select bank 1-31
		if (romBank == 0) romBank = 1; // bank 0 not valid here, maps to bank 1
		return;
	}
	if (addr >= 0x0000 && addr <= 0x1FFF && hasMBC1) {
		return; // RAM enable — ignore for now
	}

	// Serial output for Blargg test roms
	// Test rom wil write characters to 0xFF01 (serial data) and trigger transfer by writing 0x81 to 0xFF02 (serial control)
	if (addr == 0xFF02 && val == 0x81) {
		std::cout << (char)io[0x01]; // Print the character in FF01
		std::cout.flush();
	}

	// For interrupts
	if (addr == 0xFF04) {
		io[0x04] = 0; // any write resets DIV to 0
		return;
	}

	if (addr == 0xFF00) { if (joypad) joypad->write(val); return; } // If joypad is writted to and joypad is enabled, then write to joypad values and return

	if (addr <= 0x7FFF) return; // Cannot write
	if (addr <= 0x9FFF) { vram[addr - 0x8000] = val; return; }
	if (addr <= 0xBFFF) return; // Cannot write
	if (addr <= 0xCFFF) { wram[addr - 0xC000] = val; return; }
	if (addr <= 0xDFFF) { wram[addr - 0xD000 + 0x1000] = val; return; } // Will implement bank checking later
	if (addr <= 0xFE9F) { oam[addr - 0xFE00] = val; return; }
	if (addr <= 0xFF7F) { io[addr - 0xFF00] = val; return; }
	if (addr <= 0xFFFE) { hram[addr - 0xFF80] = val; return; }
	if (addr == 0xFFFF) { ie = val; return; }
}
