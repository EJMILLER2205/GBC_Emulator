#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <string>

class Bus {
public:
	uint8_t read(uint16_t addr); // Read byte
	void write(uint16_t addr, uint8_t val); // Write byte
	bool loadROM(const std::string& path); // Load rom from disk to memory
	void incrementDIV() { io[0x04]++; } // Allows timer tick to increment DIV drectly without going through the write reset, add a helper, or manipulate the io array directly in the timer

private: 
	// 8 bit data size on 16 bit address length
	std::vector<uint8_t> rom;           // Vector for any rom size
	std::array<uint8_t, 0x2000> vram{}; // 8KB vram
	std::array<uint8_t, 0x8000> wram{}; // 32KB wram
	std::array<uint8_t, 0xA0> oam{};    // 160 Bytes oam
	std::array<uint8_t, 0x80> hram{};   // 128 Bytes hram
	std::array<uint8_t, 0x80> io{};     // 128 Bytes io
	uint8_t ie = 0;                     // 1 bit interrupt enable
};
