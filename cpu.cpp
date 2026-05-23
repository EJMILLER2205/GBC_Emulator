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

void CPU::push(uint16_t val) {
	bus->write(--r.sp, val >> 8);
	bus->write(--r.sp, val & 0xFF);
}

uint16_t CPU::pop() {
	uint8_t lo = bus->read(r.sp++);
	uint8_t hi = bus->read(r.sp++);
	return ((hi << 8) | lo);
}

int CPU::step() {
	uint8_t opcode = fetch8();
	uint8_t dst = (opcode >> 3) & 0x07; // bits 5-3, 8-bit register dest
	uint8_t src = opcode & 0x07;         // bits 2-0, 8-bit register src
	uint8_t rp = (opcode >> 4) & 0x03; // bits 5-4, 16-bit register pair
	uint8_t cond = (opcode >> 3) & 0x03;

	//------------------------------------------
	// Block Zero
	//------------------------------------------
	// NOP
	if (opcode == 0x00) {
		return 4;
	}

	// ld r16, imm16
	if ((opcode & 0xCF) == 0x01) {
		if (rp == 0x00)      r.bc = fetch16();
		else if (rp == 0x01) r.de = fetch16();
		else if (rp == 0x02) r.hl = fetch16();
		else if (rp == 0x03) r.sp = fetch16();
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

	// rlca
	if (opcode == 0x07) {
		bool carry = (r.a >> 7) & 1;
		r.a = (r.a << 1) | carry;
		setFlags(false, false, false, carry);
		return 4;
	}

	// rrca
	if (opcode == 0x0F) {
		bool carry = r.a & 1;
		r.a = (r.a >> 1) | (carry << 7);
		setFlags(false, false, false, carry);
		return 4;
	}

	// rla
	if (opcode == 0x17) {
		bool carry_in = flagC();
		bool carry = (r.a >> 7) & 1;
		r.a = (r.a << 1) | carry_in;
		setFlags(false, false, false, carry);
		return 4;
	}

	// rra
	if (opcode == 0x1F) {
		bool carry_in = flagC();
		bool carry = r.a & 1;
		r.a = (r.a >> 1) | (carry_in << 7);
		setFlags(false, false, false, carry);
		return 4;
	}

	// daa
	if (opcode == 0x27) {
		uint8_t a = r.a;

		if (!flagN()) {
			// After addition
			if (flagH() || (a & 0x0F) > 9) a += 0x06;
			if (flagC() || a > 0x9F) a += 0x60;
		}
		else {
			// After subtraction
			if (flagH()) a -= 0x06;
			if (flagC()) a -= 0x60;
		}

		setFlags(a == 0, flagN(), false, flagC() || r.a > 0x99);
		r.a = a;
		return 4;
	}

	// cpl
	if (opcode == 0x2F) {
		r.a = ~r.a;
		setFlags(flagZ(), true, true, flagC());
		return 4;
	}

	// scf
	if (opcode == 0x37) {
		setFlags(flagZ(), false, false, true);
		return 4;
	}

	// ccf
	if (opcode == 0x3F) {
		setFlags(flagZ(), false, false, !flagC());
		return 4;
	}

	// jr imm8
	if (opcode == 0x18) {
		int8_t offset = static_cast<int8_t>(fetch8());
		r.pc += offset;
		return 12;
	}

	// jr cond, imm8 - if Z flag is 0
	if (opcode == 0x20) {
		int8_t offset = static_cast<int8_t>(fetch8());
		if (!flagZ()) {
			r.pc += offset;
			return 12;
		}
		else {
			return 8;
		}
	}

	// jr cond, imm8 - if Z flag is 1
	if (opcode == 0x28) {
		int8_t offset = static_cast<int8_t>(fetch8());
		if (flagZ()) {
			r.pc += offset;
			return 12;
		}
		else {
			return 8;
		}
	}

	// jr cond, imm8 - if C flag is 0
	if (opcode == 0x30) {
		int8_t offset = static_cast<int8_t>(fetch8());
		if (!flagC()) {
			r.pc += offset;
			return 12;
		}
		else {
			return 8;
		}
	}

	// jr cond, imm8 - if C flag is 1
	if (opcode == 0x38) {
		int8_t offset = static_cast<int8_t>(fetch8());
		if (flagC()) {
			r.pc += offset;
			return 12;
		}
		else {
			return 8;
		}
	}

	// stop
	if (opcode == 0x10) {
		fetch8(); // Consume the 0x00 byte that follows stop
		return 4;
	}

	//------------------------------------------
	// Block One
	//------------------------------------------
	// halt
	if (opcode == 0x76) {
		return 4;
	}

	// ld r8, r8
	if ((opcode & 0xC0) == 0x40) {
		setReg8(dst, getReg8(src));
		return (dst == 6 || src == 6) ? 8 : 4;
	}

	//------------------------------------------
	// Block Two
	//------------------------------------------
	if ((opcode & 0xC0) == 0x80) {
		uint8_t val = getReg8(src);
		int cycles = (src == 6) ? 8 : 4;

		switch (dst) {

		// add a, r8
		case 0: {
			uint16_t res = r.a + val;
			setFlags((res & 0xFF) == 0, false, ((r.a & 0x0F) + (val & 0x0F)) > 0x0F, res > 0xFF);
			r.a = res & 0xFF;
			return cycles;
		}

		// adc a, r8
		case 1: {
			uint8_t c = flagC();
			uint16_t res = r.a + val + c;
			setFlags((res & 0xFF) == 0, false, ((r.a & 0xFF) + (val & 0xFF) + c) > 0x0F, res > 0xFF);
			r.a = res & 0xFF;
			return cycles;
		}

		//sub a, r8
		case 2: {
			uint8_t res = r.a - val;
			setFlags(res == 0, true, (r.a & 0x0F) < (val & 0x0F), r.a < val);
			r.a = res;
			return cycles;
		}

		// sbc a, r8
		case 3: {
			uint8_t c = flagC();
			uint16_t res = r.a - val - c;
			setFlags((res & 0xFF) == 0, true, (r.a & 0x0F) < ((val & 0x0F) + c), r.a < (uint16_t)(val + c));
			r.a = res & 0xFF;
			return cycles;
		}

		// and a, r8
		case 4: {
			r.a &= val;
			setFlags(r.a == 0, false, true, false);
			return cycles;
		}

		// xor a, r8
		case 5: {
			r.a ^= val;
			setFlags(r.a == 0, false, false, false);
			return cycles;
		}

		// or a, r8
		case 6: {
			r.a |= val;
			setFlags(r.a == 0, false, false, false);
			return cycles;
		}

		// cp a, r8
		case 7: {
			setFlags(r.a == val, true, (r.a & 0x0F) < (val & 0x0F), r.a < val);
			return cycles;
		}
		}
	}

	//------------------------------------------
	// Block Three
	//------------------------------------------
	// add a, imm8
	if (opcode == 0xC6) {
		uint8_t val = fetch8();
		uint16_t res = r.a + val;
		setFlags((res & 0xFF) == 0, false, ((r.a & 0x0F) + (val & 0x0F)) > 0x0F, res > 0xFF);
		r.a = res & 0xFF;
		return 8;
	}

	// adc a, imm8
	if (opcode == 0xCE) {
		uint8_t val = fetch8();
		uint8_t c = flagC();
		uint16_t res = r.a + val + c;
		setFlags((res & 0xFF) == 0, false, ((r.a & 0x0F) + (val & 0x0F) + c) > 0x0F, res > 0xFF);
		r.a = res & 0xFF;
		return 8;
	}

	// sub a, imm8
	if (opcode == 0xD6) {
		uint8_t val = fetch8();
		uint8_t res = r.a - val;
		setFlags(res == 0, true, (r.a & 0x0F) < (val & 0x0F), r.a < val);
		r.a = res;
		return 8;
	}

	// sbc a, imm8
	if (opcode == 0xDE) {
		uint8_t val = fetch8();
		uint8_t c = flagC();
		uint16_t res = r.a - val - c;
		setFlags((res & 0xFF) == 0, true, (r.a & 0x0F) < ((val & 0x0F) + c), r.a < (uint16_t)(val + c));
		r.a = res & 0xFF;
		return 8;
	}

	// and a, imm8
	if (opcode == 0xE6) {
		uint8_t val = fetch8();
		r.a &= val;
		setFlags(r.a == 0, false, true, false);
		return 8;
	}

	// xor a, imm8
	if (opcode == 0xEE) {
		uint8_t val = fetch8();
		r.a ^= val;
		setFlags(r.a == 0, false, false, false);
		return 8;
	}

	// or a, imm8
	if (opcode == 0xF6) {
		uint8_t val = fetch8();
		r.a |= val;
		setFlags(r.a == 0, false, false, false);
		return 8;
	}

	// cp a, imm8
	if (opcode == 0xFE) {
		uint8_t val = fetch8();
		setFlags(r.a == val, true, (r.a & 0x0F) < (val & 0x0F), r.a < val);
		return 8;
	}

	// ret cond
	if ((opcode & 0xE7) == 0xC0) {
		if ((cond == 0) && !flagZ()) {
			r.pc = pop();
			return 20;
		}
		else if ((cond == 1) && flagZ()) {
			r.pc = pop();
			return 20;
		}
		else if ((cond == 2) && !flagC()) {
			r.pc = pop();
			return 20;
		}
		else if ((cond == 3) && flagC()) {
			r.pc = pop();
			return 20;
		}
		return 8;
	}

	// ret
	if (opcode == 0xC9) {
		r.pc = pop();
		return 16;
	}

	// reti
	if (opcode == 0xD9) {
		r.pc = pop();
		ime = true;
		return 16;
	}

	// jp cond, imm16
	if ((opcode & 0xE7) == 0xC2) {
		uint16_t addr = fetch16();
		if ((cond == 0) && !flagZ()) {
			r.pc = addr;
			return 16;
		}
		else if ((cond == 1) && flagZ()) {
			r.pc = addr;
			return 16;
		}
		else if ((cond == 2) && !flagC()) {
			r.pc = addr;
			return 16;
		}
		else if ((cond == 3) && flagC()) {
			r.pc = addr;
			return 16;
		}
		return 12;
	}

	// jp imm16
	if (opcode == 0xC3) {
		r.pc = fetch16();
		return 16;
	}

	// jp hl
	if (opcode == 0xE9) {
		r.pc = r.hl;
		return 4;
	}

	// call cond. imm16
	if ((opcode & 0xE7) == 0xC4) {
		uint16_t addr = fetch16();
		if ((cond == 0) && !flagZ()) {
			push(r.pc);
			r.pc = addr;
			return 24;
		}
		else if ((cond == 1) && flagZ()) {
			push(r.pc);
			r.pc = addr;
			return 24;
		}
		else if ((cond == 2) && !flagC()) {
			push(r.pc);
			r.pc = addr;
			return 24;
		}
		else if ((cond == 3) && flagC()) {
			push(r.pc);
			r.pc = addr;
			return 24;
		}
		return 12;
	}

	// call imm16
	if (opcode == 0xCD) {
		push(r.pc);
		r.pc = fetch16();
		return 24;
	}

	// rst tgt3
	if ((opcode & 0xC7) == 0xC7) {
		push(r.pc);
		r.pc = dst * 0x08;
		return 16;
	}

	// pop r16stk
	if ((opcode & 0xCF) == 0xC1) {
		if (rp == 0x00) {
			r.bc = pop();
		}
		else if (rp == 0x01) {
			r.de = pop();
		}
		else if (rp == 0x02) {
			r.hl = pop();
		}
		else if (rp == 0x03) {
			r.af = pop() & 0xFFF0; // Last 4 bits not usable in af
		}
		return 12;
	}

	// push r16stk
	if ((opcode & 0xCF) == 0xC5) {
		if (rp == 0x00) {
			push(r.bc);
		}
		else if (rp == 0x01) {
			push(r.de);
		}
		else if (rp == 0x02) {
			push(r.hl);
		}
		else if (rp == 0x03) {
			push(r.af);
		}
		return 16;
	}
}
