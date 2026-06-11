#include "apu.h"
#include <cmath>
#include <cstdint>
#include <SDL.h>

// Duty cycle patterns — each row is one duty cycle, 8 steps per cycle
// 1 = high (+1), 0 = low (-1)
static const uint8_t DUTY_TABLE[4][8] = {
    {0,0,0,0,0,0,0,1}, // 12.5%
    {1,0,0,0,0,0,0,1}, // 25%
    {1,0,0,0,1,1,1,1}, // 50%
    {0,1,1,1,1,1,1,0}, // 75%
};

APU::APU(Bus& bus) : bus(&bus) {
    cyclesPerSample = 4194304 / SAMPLE_RATE; // 95 CPU cycles per audio sample
    sampleBuffer.resize(BUFFER_SIZE * 8, 0.0f); // larger buffer to avoid underruns
}

void APU::tick(int cycles) {
    cycleAccum += cycles;
    while (cycleAccum >= cyclesPerSample) {
        cycleAccum -= cyclesPerSample;
        stepChannels();
        float sample = mixSamples();
        SDL_LockAudio();
        // Write stereo sample - % allows for wrapping which is easier than clearing and resetting
        sampleBuffer[writePos % sampleBuffer.size()] = sample; // Left
        writePos++;
        sampleBuffer[writePos % sampleBuffer.size()] = sample; // Right
        writePos++;
        SDL_UnlockAudio();
    }
}

void APU::triggerChannel1() {
    // Capture frequency at trigger time — only update frequency on trigger
    // to avoid clicks and pops from mid-note frequency register changes
    uint8_t nr12 = bus->read(0xFF12);
    uint8_t nr13 = bus->read(0xFF13);
    uint8_t nr14 = bus->read(0xFF14);
    ch1.frequency = ((nr14 & 0x07) << 8) | nr13;
    ch1.volume = (nr12 >> 4) & 0x0F; // capture volume at trigger time
    ch1.duty = (bus->read(0xFF11) >> 6) & 0x03;
    ch1.timer = (2048 - ch1.frequency) * 4;
    ch1.dutyPos = 0;
    ch1.active = (ch1.volume > 0);
}

void APU::triggerChannel2() {
    // Capture frequency at trigger time — only update frequency on trigger
    uint8_t nr22 = bus->read(0xFF17);
    uint8_t nr23 = bus->read(0xFF18);
    uint8_t nr24 = bus->read(0xFF19);
    ch2.frequency = ((nr24 & 0x07) << 8) | nr23;
    ch2.volume = (nr22 >> 4) & 0x0F;
    ch2.duty = (bus->read(0xFF16) >> 6) & 0x03;
    ch2.timer = (2048 - ch2.frequency) * 4;
    ch2.dutyPos = 0;
    ch2.active = (ch2.volume > 0);
}

void APU::stepChannels() {
    uint8_t nr52 = bus->read(0xFF26);

    // If sound master is off, disable all channels
    if (!(nr52 & 0x80)) {
        ch1.active = false;
        ch2.active = false;
        return;
    }

    // Advance channel 1 timer
    if (ch1.active && ch1.frequency > 0 && ch1.frequency < 2048) {
        int period = (2048 - ch1.frequency) * 4;
        ch1.timer -= cyclesPerSample;
        while (ch1.timer <= 0) {
            ch1.timer += period;
            ch1.dutyPos = (ch1.dutyPos + 1) % 8;
        }
    }

    // Advance channel 2 timer
    if (ch2.active && ch2.frequency > 0 && ch2.frequency < 2048) {
        int period = (2048 - ch2.frequency) * 4;
        ch2.timer -= cyclesPerSample;
        while (ch2.timer <= 0) {
            ch2.timer += period;
            ch2.dutyPos = (ch2.dutyPos + 1) % 8;
        }
    }
}

float APU::mixSamples() {
    float sample = 0.0f;
    int count = 0;

    // If ch1 is active and volume is on
    if (ch1.active && ch1.volume > 0) {
        float s = DUTY_TABLE[ch1.duty][ch1.dutyPos] ? 1.0f : -1.0f; // get duty table value
        sample += s * (ch1.volume / 15.0f); // scale by volume (0.0 to 1.0)
        count++;
    }

    // If ch2 is active and volume is on
    if (ch2.active && ch2.volume > 0) {
        float s = DUTY_TABLE[ch2.duty][ch2.dutyPos] ? 1.0f : -1.0f; // get duty table value
        sample += s * (ch2.volume / 15.0f); // scale by volume (0.0 to 1.0)
        count++;
    }

    // Prevents clipping — divides by number of active channels to keep output in -1 to +1 range
    if (count > 0) {
        sample /= count;
    }

    return sample * 0.15f; // master volume scaling
}

void APU::fillBuffer(float* stream, int samples) {
    for (int i = 0; i < samples; i++) {
        if (readPos != writePos) { // if buffer isn't empty
            stream[i] = sampleBuffer[readPos % sampleBuffer.size()]; // read next sample, wrap at end of buffer
            readPos++; // advance read position
        }
        else {
            stream[i] = 0.0f; // buffer underrun — output silence
        }
    }
}