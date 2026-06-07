#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

class Joypad; // Forward declaration to avoid circular dependency

class Bus {
public:
	uint8_t read(uint16_t addr); // Read byte
	void write(uint16_t addr, uint8_t val); // Write byte
	bool loadROM(const std::string& path); // Load rom from disk to memory
	void incrementDIV() { io[0x04]++; } // Allows timer tick to increment DIV drectly without going through the write reset, add a helper, or manipulate the io array directly in the timer
	void setJoypad(Joypad* jp) { joypad = jp; } // Sets the joypad up
	size_t romSize() { return rom.size(); }
	void saveRAM() {
		if (!hasRAM) return;
		std::ofstream sav(savePath, std::ios::binary);
		if (sav) {
			sav.write(reinterpret_cast<char*>(extRam.data()), extRam.size());
			std::cerr << "Saved to " << savePath << "\n";
		}
	}

private: 
	// 8 bit data size on 16 bit address length
	std::vector<uint8_t> rom;           // Vector for any rom size
	std::array<uint8_t, 0x2000> vram{}; // 8KB vram
	std::array<uint8_t, 0x8000> wram{}; // 32KB wram
	std::array<uint8_t, 0xA0> oam{};    // 160 Bytes oam
	std::array<uint8_t, 0x80> hram{};   // 128 Bytes hram
	std::array<uint8_t, 0x80> io{};     // 128 Bytes io
	uint8_t ie = 0;                     // 1 bit interrupt enable
	Joypad* joypad = nullptr; // Creates joypad

	// MBC state
	uint8_t romBank = 1; // Current ROM bank (1-127)
	uint8_t romBankMBC3 = 1; // MBC3 uses 7 bit bank number
	uint16_t romBankMBC5 = 1; // MBC5 uses 9 bit bank number
	bool hasMBC1 = false; // If the cart uses MBC1
	bool hasMBC3 = false; // If the cart uses MBC3
	bool hasMBC5 = false; // If the cart uses MBC5

	// SRAM saving
	std::array<uint8_t, 0x8000> extRam{}; // 32KB external RAM
	bool hasRAM = false;
	bool ramEnable = false;
	std::string savePath;
};
