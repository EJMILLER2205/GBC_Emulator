#pragma once
#include <cstdint>
#include "bus.h"

class Timer {
public:
	Timer(Bus& bus);
	void tick(int cycles); // Calls every cpu step with the cycles taken

private:
	Bus* bus;
	int divCycles = 0; // Total cycles of DIV
	int timaCycles = 0; // Total cycles of TIMA
};
