#include "cpu.h"
#include <stdexcept>

CPU::CPU(Bus& bus) : bus(&bus) {
	// Post boot default register states
	r.af = 0x1180;
	r.bc = 0x0000;
	r.de = 0xFF56;
	r.hl = 0x000D;
	r.sp = 0xFFFE;
	r.pc = 0x0100;
}

uint8_t CPU::fetch8() {
	return bus->read(r.pc++);
}
uint16_t CPU::fetch16() {
	uint8_t lo = fetch8();
	uint8_t hi = fetch8();
	return (hi << 8) | lo;
}

uint8_t CPU::getReg8(uint8_t id) {
	switch (id) {
	case 0: return r.b;
	case 1: return r.c;
	case 2: return r.d;
	case 3: return r.e;
	case 4: return r.h;
	case 5: return r.l;
	case 6: return bus->read(r.hl);
	case 7: return r.a;
	}
	return 0xFF;
}

void CPU::setReg8(uint8_t id, uint8_t val) {
	switch (id) {
	case 0: r.b = val; return;
	case 1: r.c = val; return;
	case 2: r.d = val; return;
	case 3: r.e = val; return;
	case 4: r.h = val; return;
	case 5: r.l = val; return;
	case 6: bus->write(r.hl, val); return;
	case 7: r.a = val; return;
	}
}

int CPU::step() {
	uint8_t opcode = fetch8();
	uint8_t dst = (opcode >> 3) & 0x07; // bits 5-3, 8-bit register dest
	uint8_t src = opcode & 0x07;         // bits 2-0, 8-bit register src
	uint8_t rp = (opcode >> 4) & 0x03; // bits 5-4, 16-bit register pair

	//------------------------------------------
	// Block One
	//------------------------------------------
	// NOP
	if (opcode == 0x00) {
		return 4;
	}

	// ld r16, imm16
	if ((opcode & 0xCF) == 0x01) {
		if (rp == 0x00)      r.bc = fetch16();
		else if (rp == 0x01) r.bc = fetch16();
		else if (rp == 0x02) r.de = fetch16();
		else if (rp == 0x03) r.hl = fetch16();
		return 12;
	}

	// ld [r16mem], a
	if ((opcode & 0xCF) == 0x02) {
		if (rp == 0x00)      bus->write(r.bc, r.a);
		else if (rp == 0x01) bus->write(r.de, r.a);
		else if (rp == 0x02) bus->write(r.hl++, r.a);
		else if (rp == 0x03) bus->write(r.hl--, r.a);
		return 8;
	}

	// ld a, [r16mem]
	if ((opcode & 0xCF) == 0x0A) {
		if (rp == 0x00) r.a = bus->read(r.bc);
		else if (rp == 0x01) r.a = bus->read(r.de);
		else if (rp == 0x02) r.a = bus->read(r.hl++);
		else if (rp == 0x03) r.a = bus->read(r.hl--);
		return 8;
	}

	// ld [imm16], sp
	if (opcode == 0x08) {
		uint16_t addr = fetch16();
		bus->write(addr, r.sp & 0x00FF);
		bus->write(addr + 1, r.sp >> 8);
		return 20;
	}

	// inc r16
	if ((opcode & 0xCF) == 0x03) {
		if (rp == 0x00) r.bc++;
		else if (rp == 0x01) r.de++;
		else if (rp == 0x02) r.hl++;
		else if (rp == 0x03) r.sp++;
		return 8;
	}

	// dec r16
	if ((opcode & 0xCF) == 0x0B) {
		if (rp == 0x00) r.bc--;
		else if (rp == 0x01) r.de--;
		else if (rp == 0x02) r.hl--;
		else if (rp == 0x03) r.sp--;
		return 8;
	}

	// add hl, r16
	if ((opcode & 0xCF) == 0x09) {
		uint16_t value;
		if (rp == 0x00) value = r.bc;
		else if (rp == 0x01) value = r.de;
		else if (rp == 0x02) value = r.hl;
		else value = r.sp;

		uint32_t result = r.hl + value;
		setFlags(flagZ(), false, ((r.hl & 0xFFF) + (value & 0xFFF)) > 0xFFF, result > 0xFFFF);
		r.hl = result & 0x0000FFFF;
		return 8;
	}

	// inc r8
	if ((opcode & 0xC7) == 0x04) {
		uint8_t val = getReg8(dst);
		uint8_t res = val + 1;
		setReg8(dst, res);
		setFlags(res == 0, false, ((val & 0xF) == 0xF), flagC());
		return (dst == 6) ? 12 : 4; // Cylces is 12 when working with hl, 4 when otherwise
	}

	// dec r8
	if ((opcode & 0xC7) == 0x05) {
		uint8_t val = getReg8(dst);
		uint8_t res = val - 1;
		setReg8(dst, res);
		setFlags(res == 0, true, ((val & 0xF) == 0x0), flagC());
		return (dst == 6) ? 12 : 4; // Cylces is 12 when working with hl, 4 when otherwise
	}

	// ld r8, imm8
	if ((opcode & 0xC7) == 0x06) {
		setReg8(dst, fetch8());
		return (dst == 6) ? 12 : 8; // Cycles is 12 when working with hl, 8 when otherwise
	}

}