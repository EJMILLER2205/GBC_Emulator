#pragma once
#include "bus.h"
#include <cstdint>
#include <vector>

// pulseChannel struct
struct pulseChannel {
	// Pulse channel data
	uint16_t freq_register = 0;
	uint32_t freq_inc = 0;
	uint32_t freq_counter = 0;
	uint8_t duty = 0;
	uint8_t duty_counter = 0;
	uint8_t volume = 0;
	bool enabled = false;

	// Frame sequencer components
	uint8_t env_volume = 0;
	bool env_dir = false;
	uint8_t env_period = 0;
	uint8_t env_timer = 0;
	int length_counter = 0;
	bool length_enable = false;
};

// noiseChannel struct
struct noiseChannel {
	bool enabled = false;
	uint8_t volume = 0;
	uint8_t env_volume = 0;
	bool env_dir = false;
	uint8_t env_period = 0;
	uint8_t env_timer = 0;
	int length_counter = 0;
	bool length_enable = false;
	uint16_t lfsr = 0x7FFF; // 15-bit shift register, starts all 1s
	bool lfsr_wide = true; // true = 15-bit, false = 7-bit
	uint32_t freq_inc = 0; // frequency increment
	uint32_t freq_counter = 0; // frequency counter
	int8_t output = 0; // current output value (+1 or -1)
};

// waveChanel struct
struct waveChannel {
	bool enabled = false;
	uint8_t volume_shift = 0; // 0=mute, 1=100%, 2=50%, 3=25%
	uint16_t freq_register = 0;
	uint32_t freq_inc = 0;
	uint32_t freq_counter = 0;
	uint8_t position = 0; // current position in wave RAM (0-31)
	int length_counter = 0;
	bool length_enable = false;
	int8_t output = 0; // current output sample
};


// APU class
class APU {
public:
	APU(Bus& bus);
	void tick(int cycles);
	void fillBuffer(float* stream, int samples);
	void triggerChannel1();
	void triggerChannel2();
	void triggerChannel3();
	void triggerChannel4();

	// Constants
	static const int SAMPLE_RATE = 44100;
	static const int BUFFER_SIZE = 2048;
	static const int FRAME_SEQ_PERIOD = 8192; // 4194304 / 512 = 8192 cycles per step

private:
	Bus* bus;

	void stepChannels();
	void clockFrameSequencer();
	void clockLengthCounters();
	void clockVolumeEnvelopes();
	float mixSamples();
	void setNoteFreq(pulseChannel& c); // calculates freq_inc from freq_register
	void stepChannel3();
	void stepChannel4();

	pulseChannel ch1, ch2; // Both channels
	noiseChannel ch4;
	waveChannel  ch3;
	std::vector<float> sampleBuffer; // circular audio buffer
	int writePos = 0;
	int readPos = 0;
	int cycleAccum = 0;
	int cyclesPerSample = 0;

	// Frame sequencer
	int frameSeqCycles = 0;
	int frameSeqStep = 0;

};
