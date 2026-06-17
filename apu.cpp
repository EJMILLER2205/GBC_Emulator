#include "apu.h"
#include <cstdint>

// Fixed point scaling constant
// SAMPLE_RATE * 105 gives clean integer values for envelope periods
// freq_counter overflows this value to advance duty position
static const uint32_t FREQ_INC_REF = APU::SAMPLE_RATE * 105;

APU::APU(Bus& bus) : bus(&bus) {
	// Calculate cycles per sample
	cyclesPerSample = 4194304 / SAMPLE_RATE; // Gameboy rusns at 4194304 Hz, so  4194304 / 44100  = 95 (every 95 CPU cycles we generate an audio sample
	// Initialize the sample buffer
	sampleBuffer.resize(BUFFER_SIZE * 4, 0.0f); // Allows sample buffer to hold enough sampels to prevent underruns
}

void APU::setNoteFreq(pulseChannel& c) {
	uint32_t freq = (4194304 / 4) / (2048 - c.freq_register); // Gets actual frequency
	c.freq_inc = freq * (FREQ_INC_REF / SAMPLE_RATE); // Scale by freq_inc / SAMPLE_RATE
}

void APU::triggerChannel1() {
	// Read channel 1 registers from bus
	uint8_t nr11 = bus->read(0xFF11); // duty and length
	uint8_t nr12 = bus->read(0xFF12); // volume envelope
	uint8_t nr13 = bus->read(0xFF13); // frequency low byte
	uint8_t nr14 = bus->read(0xFF14); // frequency high byte + trigger

	// Set frequency (combine high 3 bits of nr14 with all 8 bits of nr13)
	ch1.freq_register = ((nr14 & 0x07) << 8) | nr13;
	setNoteFreq(ch1); // calculate freq_inc from freq_registger

	// Reset frequency counter and duty position
	ch1.freq_counter = 0;
	ch1.duty_counter = 0;

	// Set duty cycle pattern (bits 7-6 of nr11)
	ch1.duty = (nr11 >> 6) & 0x03;

	// Set volume envelope
	ch1.env_volume = (nr12 >> 4) & 0x0F; // initial volume (bits 7-4)
	ch1.env_dir = (nr12 >> 3) & 0x01; // direction (bit 3)
	ch1.env_period = nr12 & 0x07; // period (bits 2-0)
	ch1.env_timer = ch1.env_period; // reset envelope timer
	ch1.volume = ch1.env_volume; // set current volume to initial

	// Set length counter (bits 5-0 of nr11)
	ch1.length_enable = (nr14 >> 6) & 0x01;
	ch1.length_counter = 64 - (nr11 & 0x3F);

	// Activate channel if volume > 0 or envelope is increasing
	ch1.enabled = (ch1.volume > 0 || ch1.env_dir);
}

void APU::triggerChannel2() {
	// Read channel 2 registers from bus
	uint8_t nr21 = bus->read(0xFF16); // duty and length
	uint8_t nr22 = bus->read(0xFF17); // volume envelope
	uint8_t nr23 = bus->read(0xFF18); // frequency low byte
	uint8_t nr24 = bus->read(0xFF19); // frequency high byte + trigger

	// Set frequency (combine high 3 bits of nr14 with all 8 bits of nr13)
	ch2.freq_register = ((nr24 & 0x07) << 8) | nr23;
	setNoteFreq(ch2); // calculate freq_inc from freq_registger

	// Reset frequency counter and duty position
	ch2.freq_counter = 0;
	ch2.duty_counter = 0;

	// Set duty cycle pattern (bits 7-6 of nr11)
	ch2.duty = (nr21 >> 6) & 0x03;

	// Set volume envelope
	ch2.env_volume = (nr22 >> 4) & 0x0F; // initial volume (bits 7-4)
	ch2.env_dir = (nr22 >> 3) & 0x01; // direction (bit 3)
	ch2.env_period = nr22 & 0x07; // period (bits 2-0)
	ch2.env_timer = ch2.env_period; // reset envelope timer
	ch2.volume = ch2.env_volume; // set current volume to initial

	// Set length counter (bits 5-0 of nr11)
	ch2.length_enable = (nr24 >> 6) & 0x01;
	ch2.length_counter = 64 - (nr21 & 0x3F);

	// Activate channel if volume > 0 or envelope is increasing
	ch2.enabled = (ch2.volume > 0 || ch2.env_dir);
}

void APU::stepChannels() {
	// Check if sound master is on (nr52 bit 7)
	uint8_t nr52 = bus->read(0xFF26);
	if (!(nr52 & 0x80)) {
		// Sound master off - disable all channels
		ch1.enabled = false;
		ch2.enabled = false;
		return;
	}

	// Advance channel 1 frequency counter
	if (ch1.enabled && ch1.freq_register > 0 && ch1.freq_register < 2048) {
		ch1.freq_counter += ch1.freq_inc;
		// Each time freq_counter overflows FREQ_INC_REF, advance duty positon
		while (ch1.freq_counter >= FREQ_INC_REF) {
			ch1.freq_counter -= FREQ_INC_REF;
			ch1.duty_counter = (ch1.duty_counter + 1) & 7; // wrap at 8
		}
	}

	// Advance channel 2 frequency counter
	if (ch2.enabled && ch2.freq_register > 0 && ch2.freq_register < 2048) {
		ch2.freq_counter += ch2.freq_inc;
		// Each time freq_counter overflows FREQ_INC_REF, advance duty positon
		while (ch2.freq_counter >= FREQ_INC_REF) {
			ch2.freq_counter -= FREQ_INC_REF;
			ch2.duty_counter = (ch2.duty_counter + 1) & 7;
		}
	}
	stepChannel3();
	stepChannel4();
}

// Duty cycle patterns — 1 = high (+1), 0 = low (-1)
// Each row is one duty pattern, 8 steps per cycle
static const uint8_t DUTY_TABLE[4][8] = {
	{ 0,0,0,0,0,0,0,1 }, // 12.5%
	{ 1,0,0,0,0,0,0,1 }, // 25%
	{ 1,0,0,0,1,1,1,1 }, // 50%
	{ 0,1,1,1,1,1,1,0 }, // 75%
};

float APU::mixSamples() {
	float sample = 0.0f;
	int count = 0;

	// Channel 1 — pulse
	if (ch1.enabled && ch1.volume > 0) {
		float s = DUTY_TABLE[ch1.duty][ch1.duty_counter] ? 1.0f : -1.0f;
		sample += s * (ch1.volume / 15.0f);
		count++;
	}

	// Channel 2 — pulse
	if (ch2.enabled && ch2.volume > 0) {
		float s = DUTY_TABLE[ch2.duty][ch2.duty_counter] ? 1.0f : -1.0f;
		sample += s * (ch2.volume / 15.0f);
		count++;
	}

	// Channel 3 — wave
	if (ch3.enabled && ch3.volume_shift > 0) {
		// Wave output is 0-15, center it around 0 and normalize
		float s = (ch3.output - 7.5f) / 7.5f;
		sample += s;
		count++;
	}

	// Channel 4 — noise
	if (ch4.enabled && ch4.volume > 0) {
		float s = (float)ch4.output; // already +1 or -1
		sample += s * (ch4.volume / 15.0f);
		count++;
	}

	if (count > 0) sample /= count;

	// Low pass filter to reduce harshness
	static float prev = 0.0f;
	float filtered = 0.5f * sample + 0.5f * prev;
	prev = sample;

	return filtered * 0.25f;
}

void APU::clockLengthCounters() {
	// Decrement length counter — silence channel when it reaches 0
	// Only active when length_enable is set (NR14/NR24 bit 6)
	// Channel 1 length
	if (ch1.enabled && ch1.length_enable) {
		if (ch1.length_counter > 0) {
			ch1.length_counter--;
			if (ch1.length_counter == 0) {
				ch1.enabled = false; // silence channel
			}
		}
	}

	// Channel 2 length
	if (ch2.enabled && ch2.length_enable) {
		if (ch2.length_counter > 0) {
			ch2.length_counter--;
			if (ch2.length_counter == 0) {
				ch2.enabled = false;
			}
		}
	}

	// Channel 3 length
	if (ch3.enabled && ch3.length_enable) {
		if (ch3.length_counter > 0) {
			ch3.length_counter--;
			if (ch3.length_counter == 0) {
				ch3.enabled = false;
			}
		}
	}

	// Channel 4 length
	if (ch4.enabled && ch4.length_enable) {
		if (ch4.length_counter > 0) {
			ch4.length_counter--;
			if (ch4.length_counter == 0) {
				ch4.enabled = false;
			}
		}
	}
}

void APU::clockVolumeEnvelopes() {
	// Channel 1 envelope
	// env_period 0 means envelope is disabled
	if (ch1.enabled && ch1.env_period > 0) {
		ch1.env_timer--;
		if (ch1.env_timer == 0) {
			// Reload timer
			ch1.env_timer = ch1.env_period;
			// Increase or decrease volume
			if (ch1.env_dir && ch1.volume < 15) {
				ch1.volume++; // increasing envelope
			}
			else if (!ch1.env_dir && ch1.volume > 0) {
				ch1.volume--; // decreasing envelope
				if (ch1.volume == 0) {
					ch1.enabled = false; // silence when volume hits 0
				}
			}
		}
	}

	// Channel 2 envelope
	if (ch2.enabled && ch2.env_period > 0) {
		ch2.env_timer--;
		if (ch2.env_timer == 0) {
			ch2.env_timer = ch2.env_period;
			if (ch2.env_dir && ch2.volume < 15) {
				ch2.volume++;
			}
			else if (!ch2.env_dir && ch2.volume > 0) {
				ch2.volume--;
				if (ch2.volume == 0) {
					ch2.enabled = false;
				}
			}
		}
	}

	// Channel 4 envelope (channel 3 has no envelope)
	if (ch4.enabled && ch4.env_period > 0) {
		ch4.env_timer--;
		if (ch4.env_timer == 0) {
			ch4.env_timer = ch4.env_period;
			if (ch4.env_dir && ch4.volume < 15) {
				ch4.volume++;
			}
			else if (!ch4.env_dir && ch4.volume > 0) {
				ch4.volume--;
				if (ch4.volume == 0) ch4.enabled = false;
			}
		}
	}
}

void APU::clockFrameSequencer() {
	// Frame sequencer runs at 512Hz with 8 steps
	// Step 0,2,4,6 - clock length counters (256Hz)
	// Step 7 - clock volume envelopes (64Hz)
	switch (frameSeqStep) {
	case 0: clockLengthCounters(); break;
	case 2: clockLengthCounters(); break;
	case 4: clockLengthCounters(); break;
	case 6: clockLengthCounters(); break;
	case 7: clockVolumeEnvelopes(); break;
	}

	// Advance to next step, wrap at 8
	frameSeqStep = (frameSeqStep + 1) % 8;
}

void APU::tick(int cycles) {
	frameSeqCycles += cycles;
	while (frameSeqCycles >= FRAME_SEQ_PERIOD) {
		frameSeqCycles -= FRAME_SEQ_PERIOD;
		clockFrameSequencer();
	}

	cycleAccum += cycles;
	while (cycleAccum >= cyclesPerSample) {
		cycleAccum -= cyclesPerSample;
		stepChannels();
		float sample = mixSamples();

		// Only write if buffer isn't full
		int bufSize = (int)sampleBuffer.size();
		int used = ((writePos - readPos) + bufSize) % bufSize;
		if (used < bufSize - 4) {
			sampleBuffer[writePos % bufSize] = sample;
			writePos++;
			sampleBuffer[writePos % bufSize] = sample;
			writePos++;
		}
	}
}

void APU::fillBuffer(float* stream, int samples) {
	for (int i = 0; i < samples; i++) {
		if (readPos != writePos) {
			// Read next sample from circular buffer
			stream[i] = sampleBuffer[readPos % sampleBuffer.size()];
			readPos++;
		}
		else {
			// Buffer underrun — output silence
			stream[i] = 0.0f;
		}
	}
}

void APU::triggerChannel3() {
	uint8_t nr30 = bus->read(0xFF1A); // channel 3 enable
	uint8_t nr31 = bus->read(0xFF1B); // length
	uint8_t nr32 = bus->read(0xFF1C); // volume
	uint8_t nr33 = bus->read(0xFF1D); // frequency low
	uint8_t nr34 = bus->read(0xFF1E); // frequency high + trigger

	// Only active if bit 7 of NR30 is set
	if (!(nr30 & 0x80)) {
		ch3.enabled = false;
		return;
	}

	// Set frequency — wave channel uses same 11-bit register
	ch3.freq_register = ((nr34 & 0x07) << 8) | nr33;

	// Wave channel freq_inc uses /2 instead of /4 (runs at double speed)
	uint32_t freq = (4194304 / 2) / (2048 - ch3.freq_register);
	ch3.freq_inc = freq * (FREQ_INC_REF / SAMPLE_RATE);

	ch3.freq_counter = 0;
	ch3.position = 0;

	// Volume shift — NR32 bits 6-5
	// 00=mute, 01=100%, 10=50%, 11=25%
	ch3.volume_shift = (nr32 >> 5) & 0x03;

	// Length counter
	ch3.length_enable = (nr34 >> 6) & 0x01;
	ch3.length_counter = 256 - nr31; // wave uses 256 not 64

	ch3.enabled = (ch3.volume_shift > 0);
}

void APU::triggerChannel4() {
	uint8_t nr41 = bus->read(0xFF20); // length
	uint8_t nr42 = bus->read(0xFF21); // volume envelope
	uint8_t nr43 = bus->read(0xFF22); // frequency/randomness
	uint8_t nr44 = bus->read(0xFF23); // trigger + length enable

	// Volume envelope
	ch4.env_volume = (nr42 >> 4) & 0x0F;
	ch4.env_dir = (nr42 >> 3) & 0x01;
	ch4.env_period = nr42 & 0x07;
	ch4.env_timer = ch4.env_period;
	ch4.volume = ch4.env_volume;

	// LFSR mode — bit 3 of NR43
	// false = 15-bit (white noise), true = 7-bit (tonal noise)
	ch4.lfsr_wide = !((nr43 >> 3) & 0x01);
	ch4.lfsr = 0x7FFF; // reset shift register

	// Calculate frequency from NR43
	// Divisor from bits 2-0, shift from bits 7-4
	static const uint8_t divisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };
	uint8_t div = divisors[nr43 & 0x07];
	uint8_t shift = (nr43 >> 4) & 0x0F;
	uint32_t freq = (4194304 / div) >> shift;
	ch4.freq_inc = freq * (FREQ_INC_REF / SAMPLE_RATE);

	ch4.freq_counter = 0;
	ch4.output = 1; // start high

	// Length counter
	ch4.length_enable = (nr44 >> 6) & 0x01;
	ch4.length_counter = 64 - (nr41 & 0x3F);

	ch4.enabled = (ch4.volume > 0 || ch4.env_dir);
}

void APU::stepChannel3() {
	if (!ch3.enabled || ch3.freq_register == 0) return;

	ch3.freq_counter += ch3.freq_inc;
	while (ch3.freq_counter >= FREQ_INC_REF) {
		ch3.freq_counter -= FREQ_INC_REF;

		// Advance position in wave RAM (0-31)
		ch3.position = (ch3.position + 1) & 31;

		// Read nibble from wave RAM
		uint8_t byte = bus->read(0xFF30 + (ch3.position / 2));
		uint8_t nibble;
		if (ch3.position & 1) {
			nibble = byte & 0x0F;        // low nibble
		}
		else {
			nibble = (byte >> 4) & 0x0F; // high nibble
		}

		// Apply volume shift
		// volume_shift 0=mute, 1=100%, 2=50%, 3=25%
		if (ch3.volume_shift == 0) {
			ch3.output = 0;
		}
		else {
			ch3.output = (nibble >> (ch3.volume_shift - 1));
		}
	}
}

void APU::stepChannel4() {
	if (!ch4.enabled) return;

	ch4.freq_counter += ch4.freq_inc;
	while (ch4.freq_counter >= FREQ_INC_REF) {
		ch4.freq_counter -= FREQ_INC_REF;

		// Clock the LFSR
		// XOR bit 1 and bit 0
		uint8_t xor_result = (ch4.lfsr & 0x01) ^ ((ch4.lfsr >> 1) & 0x01);

		// Shift right and put XOR result in bit 14
		ch4.lfsr = (ch4.lfsr >> 1) | (xor_result << 14);

		// In 7-bit mode also put XOR result in bit 6
		if (!ch4.lfsr_wide) {
			ch4.lfsr = (ch4.lfsr & ~0x40) | (xor_result << 6);
		}

		// Output is inverse of bit 0
		ch4.output = (ch4.lfsr & 0x01) ? -1 : 1;
	}
}
