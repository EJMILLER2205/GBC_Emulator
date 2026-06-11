#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include "bus.h"

class APU {
public:
    APU(Bus& bus);
    void tick(int cycles);          // Called every CPU step
    void fillBuffer(float* stream, int samples); // called by SDL to fill audio buffer
    void triggerChannel1();         // Called by bus when NR14 is written with trigger bit
    void triggerChannel2();         // Called by bus when NR24 is written with trigger bit
    static const int SAMPLE_RATE = 44100;  // SDL audio Hz
    static const int BUFFER_SIZE = 2048;   // samples per callback

private:
    Bus* bus;

    // Sample generation
    int cycleAccum = 0;      // Accumulated cycles
    int cyclesPerSample;     // CPU cycles per audio sample

    std::vector<float> sampleBuffer;
    int writePos = 0;
    int readPos = 0;

    // Pulse channel struct — used for both channel 1 and channel 2
    struct PulseChannel {
        bool    active = false; // true when channel has been triggered
        uint8_t duty = 0;    // duty cycle 0-3
        int     frequency = 0;    // 11-bit frequency value, captured at trigger time
        uint8_t volume = 0;    // 0-15
        int     timer = 0;    // frequency timer, counts down to advance duty position
        int     dutyPos = 0;    // position in duty cycle (0-7)
    } ch1, ch2;

    void stepChannels();
    float mixSamples();
};