#pragma once
#include <cstdint>
#include "bus.h"

class CPU {
public:
	CPU(Bus& bus); // Constructor
	int step();    // Returns cycles taken

private:

	// Registers structure
	struct Regs {
		// Creates more unions for each combined 16-bit registers from indicidual 8 bit registers
		// These include AF BC DE HL SP and PC (SP and PC are not made up of other smaller registers)

		// AF
		union {
			struct {
				uint8_t f, a;
			};
			uint16_t af;
		};

		// BC
		union {
			struct {
				uint8_t c, b;
			};
			uint16_t bc;
		};

		// DE
		union {
			struct {
				uint8_t e, d;
			};
			uint16_t de;
		};

		// HL
		union {
			struct {
				uint8_t l, h;
			};
			uint16_t hl;
		};

		uint16_t sp = 0xFFFE; // Stack pointer set to highest value of usable memory cause stack grows downwards
		uint16_t pc = 0x0100; // 0x0100 is entry program counter point after bootup rom
	} r;

	// Flag accessors
	bool flagZ() const { return (r.f >> 7) & 1; }
	bool flagN() const { return (r.f >> 6) & 1; }
	bool flagH() const { return (r.f >> 5) & 1; }
	bool flagC() const { return (r.f >> 4) & 1; }
	void setFlags(bool z, bool n, bool h, bool c) {
		r.f = (z << 7) | (n << 6) | (h << 5) | (c << 4); // | (r.f & 0x0F) not needed cause bottom 4 bits dont hold meaningful data and should be 0
	}

	// Halt flag
	bool halted = false;

	// Interrupt enable
	bool ime = false;
	bool ime_pending = false;

	// Fetch helpers
	uint8_t fetch8();   // Reads one byte at Pc, then PC++
	uint16_t fetch16(); // Reads two bytes at PC and PC + 1, then PC += 2;

	// Register helpers
	uint8_t getReg8(uint8_t id);
	void setReg8(uint8_t id, uint8_t val);

	//Push and pop helpers
	void push(uint16_t val);
	uint16_t pop();

	// Pointer to bus object to call things like bus->read(), etc
	Bus* bus;
};
