#include "joypad.h"

Joypad::Joypad(Bus& bus) : bus(&bus) {}

void Joypad::handleEvent(SDL_Event& e) {
	if (e.type == SDL_KEYDOWN) {
		switch (e.key.keysym.sym) {
		case SDLK_RIGHT: right = true; break;
		case SDLK_LEFT: left = true; break;
		case SDLK_UP: up = true; break;
		case SDLK_DOWN: down = true; break;
		case SDLK_z: a = true; break;
		case SDLK_x: b = true; break;
		case SDLK_RETURN: start = true; break;
		case SDLK_BACKSPACE: select_btn = true; break;
		}
	}

	if (e.type == SDL_KEYUP) {
		switch (e.key.keysym.sym) {
		case SDLK_RIGHT: right = false; break;
		case SDLK_LEFT: left = false; break;
		case SDLK_UP: up = false; break;
		case SDLK_DOWN: down = false; break;
		case SDLK_z: a = false; break;
		case SDLK_x: b = false; break;
		case SDLK_RETURN: start = false; break;
		case SDLK_BACKSPACE: select_btn = false; break;
		}
	}
}

uint8_t Joypad::read() {
	uint8_t result = 0xFF; // Start with all bits high (not pressed due to active low)

	if (!(select & 0x20)) {
		// Button group selected
		if (a) result &= ~0x01;
		if (b) result &= ~0x02;
		if (select_btn) result &= ~0x04;
		if (start) result &= ~0x08;
	}

	if (!(select & 0x10)) {
		// D-pad group selected
		if (right) result &= ~0x01;
		if (left) result &= ~0x02;
		if (up) result &= ~0x04;
		if (down) result &= ~0x08;
	}
	return result;
}

void Joypad::write(uint8_t val) {
	select = val;
}
