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
		hasMBC3 = (cartType >= 0x0F && cartType <= 0x13);
		hasMBC5 = (cartType >= 0x19 && cartType <= 0x1E);
		std::cerr << "Cart type: 0x" << std::hex << (int)cartType << "\n";

		// Check if cart has RAM
		hasRAM = (cartType == 0x02 || cartType == 0x03 ||
			cartType == 0x0C || cartType == 0x0D ||
			cartType == 0x10 || cartType == 0x12 ||
			cartType == 0x13 || cartType == 0x1A ||
			cartType == 0x1B || cartType == 0x1D ||
			cartType == 0x1E);

		// Set save path
		savePath = path.substr(0, path.find_last_of('.')) + ".sav";

		// Load existing save if it exists
		if (hasRAM) {
			std::ifstream sav(savePath, std::ios::binary);
			if (sav) {
				sav.read(reinterpret_cast<char*>(extRam.data()), extRam.size());
				std::cerr << "Loaded save from " << savePath << "\n";
			}
		}
	}

	// Default pallete values to simulate boot ROM
	io[0x47] = 0xFC; // BGP
	io[0x48] = 0xFF; // OBP0
	io[0x49] = 0xFF; // OBP1

	return true;
}

uint8_t Bus::read(uint16_t addr) {
	// ROM bank 0 — always fixed
	if (addr <= 0x3FFF) return addr < rom.size() ? rom[addr] : 0xFF;

	// ROM bank 1+ — switchable
	if (addr <= 0x7FFF) {
		uint32_t bank;
		if (hasMBC5)      bank = romBankMBC5;
		else if (hasMBC3) bank = romBankMBC3;
		else              bank = romBank;
		uint32_t bankAddr = (bank * 0x4000) + (addr - 0x4000);
		return bankAddr < rom.size() ? rom[bankAddr] : 0xFF;
	}

	if (addr <= 0x9FFF) return vram[addr - 0x8000];

	// Read external Ram if there is SRAM enabled
	if (addr <= 0xBFFF) {
		if (hasRAM && ramEnable) {
			return extRam[addr - 0xA000];
		}
		return 0xFF;
	}
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
	// OAM DMA transfer (used for copying blocks of memory quickly without the CPU going byte by byte)
	if (addr == 0xFF46) {
		uint16_t src = val << 8; // source address = val * 0x100
		for (int i = 0; i < 0xA0; i++) {
			oam[i] = read(src + i);
		}
		return;
	}
	// MBC5 bank switching
	if (hasMBC5) {
		if (addr >= 0x0000 && addr <= 0x1FFF) return;
		if (addr >= 0x2000 && addr <= 0x2FFF) {
			romBankMBC5 = (romBankMBC5 & 0x100) | val;
			return;
		}
		if (addr >= 0x3000 && addr <= 0x3FFF) {
			romBankMBC5 = (romBankMBC5 & 0xFF) | ((val & 0x01) << 8);
			return;
		}
		if (addr >= 0x4000 && addr <= 0x5FFF) return;
	}

	// MBC3 bank switching
	if (hasMBC3) {
		// SRAM check
		if (addr >= 0x0000 && addr <= 0x1FFF && hasMBC3) {
			ramEnable = (val & 0x0F) == 0x0A;
			return;
		}
		if (addr >= 0x2000 && addr <= 0x3FFF) {
			// 7-bit ROM bank number
			romBankMBC3 = val & 0x7F;
			if (romBankMBC3 == 0) romBankMBC3 = 1; // bank 0 maps to bank 1
			return;
		}
		if (addr >= 0x4000 && addr <= 0x5FFF) return; // RAM bank / RTC select
		if (addr >= 0x6000 && addr <= 0x7FFF) return; // RTC latch
	}

	// MBC1 bank switching
	if (hasMBC1) {
		// SRAM check
		if (addr >= 0x0000 && addr <= 0x1FFF && hasMBC1) {
			ramEnable = (val & 0x0F) == 0x0A;
			return;
		}
		if (addr >= 0x2000 && addr <= 0x3FFF) {
			romBank = val & 0x1F;
			return;
		}
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

	// Write external Ram if SRAM is enabled
	if (addr <= 0xBFFF) {
		if (hasRAM && ramEnable) {
			extRam[addr - 0xA000] = val;
		}
		return;
	}
	if (addr <= 0xCFFF) { wram[addr - 0xC000] = val; return; }
	if (addr <= 0xDFFF) { wram[addr - 0xD000 + 0x1000] = val; return; } // Will implement bank checking later
	if (addr <= 0xFE9F) { oam[addr - 0xFE00] = val; return; }
	if (addr <= 0xFF7F) { io[addr - 0xFF00] = val; return; }
	if (addr <= 0xFFFE) { hram[addr - 0xFF80] = val; return; }
	if (addr == 0xFFFF) { ie = val; return; }
}
