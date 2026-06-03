#include "timer.h"

Timer::Timer(Bus& bus) : bus(&bus) {}

void Timer::tick(int cycles) {
    // DIV increments every 256 cycles
    divCycles += cycles;
    while (divCycles >= 256) {
        divCycles -= 256;
        bus->incrementDIV();
    }

    // Check if timer is enabled (TAC bit 2)
    uint8_t tac = bus->read(0xFF07);
    if (!(tac & 0x04)) return; // timer disabled, stop here

    // Get TIMA increment frequency from TAC bits 1-0
    static const int freqs[] = { 1024, 16, 64, 256 };
    int freq = freqs[tac & 0x03];

    // Accumulate cycles for TIMA
    timaCycles += cycles;
    while (timaCycles >= freq) {
        timaCycles -= freq;

        uint8_t tima = bus->read(0xFF05);
        if (tima == 0xFF) {
            // TIMA overflowed — reset to TMA and request timer interrupt
            bus->write(0xFF05, bus->read(0xFF06)); // TIMA = TMA
            uint8_t IF = bus->read(0xFF0F);
            bus->write(0xFF0F, IF | 0x04);         // set bit 2 of IF
        }
        else {
            bus->write(0xFF05, tima + 1);
        }
    }
}
