#include <SDL.h>
#include <iostream>
#include "bus.h"
#include "cpu.h"

int main(int argc, char* argv[]) { // These arguments in main required for SDL2 to link properly

	// Checks to make sure that display/window system was properly initialized
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cerr << "SDL Init failed: " << SDL_GetError() << "\n";
		return 1;
	}

	//Create a window for the display
	SDL_Window* window = SDL_CreateWindow(				//This is a pointer becuase it points to the location in the SDL2 memory (on heap)
		"GBC Emulator",									// Title bar
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // Centers window on screen
		160 * 3, 144 * 3,								// Width and Height in pixels (scaled by 3 and 4 so that it is visible on modern displays)
		SDL_WINDOW_SHOWN								// Makes the window visible
	);

	// Creates a renderer (object responisible for drawing to window) with -1(select best graphics driver), and SDL_RENDERER_ACCELERATED(use the gpu rather than software rendering)
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED); 

	// Creates bus object and loads ROM
	Bus bus;
	if (!bus.loadROM("roms/tetris.gb")) {
		std::cerr << "Failed to load ROM\n";
		return 1;
	}

	// Creates CPU object
	CPU cpu(bus);

	bool running = true; // Functions as kill switch for the main loop
	SDL_Event e;		 // Union struct that stores whatever most recently happened (key press, mouse move, window close, etc)

	// The main loop, ends when kill switch is set to false
	while (running) {
		// Pulls one event off SDL's internal queue each call, fills the e struct with it, and returns 1 if there was an event and 0 if queue is empty
		while (SDL_PollEvent(&e)) {
			//If window is closed, call kill switch
			if (e.type == SDL_QUIT) {
				running = false;
			}
		}

		// Run one frame wortht of cycles (~70224 cycles at 60fps)
		int cycles = 0;
		while (cycles < 70224) {
			cycles += cpu.step();
		}

		SDL_SetRenderDrawColor(renderer, 15, 56, 15, 255); // Sets active color (RGBA) to the default gameboy green
		SDL_RenderClear(renderer);						   // Fills the entire window with the selected color, wiping previous frame
		SDL_RenderPresent(renderer);					   // Flips the back buffer to the screen (double buffering). Nothing is visible until this is called
	}

	//Destroy in reverse order of creation
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}