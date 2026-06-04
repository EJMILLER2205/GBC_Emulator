#pragma once
#include <cstdint>
#include <SDL.h>
#include "bus.h"

class Joypad {
public:
	Joypad(Bus& bus);
	void handleEvent(SDL_Event& e); // call from main event loop
	uint8_t read(); // called by bus when 0xFF00 is read
	void write(uint8_t val); // called by bus when 0xFF00 is written

private:
	Bus* bus;
	uint8_t select = 0xFF; // which group is selected (written by game)

	// Button states - true is pressed
	bool right = false;
	bool left = false;
	bool up = false;
	bool down = false;
	bool a = false;
	bool b = false;
	bool start = false;
	bool select_btn = false;
};