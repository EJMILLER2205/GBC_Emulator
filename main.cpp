#include <SDL.h>
#include <iostream>
#include "bus.h"
#include "cpu.h"
#include <stdexcept>
#include <windows.h>
#include "timer.h"
#include "ppu.h"
#include <vector>
#include "joypad.h"
#include <commdlg.h> // Windows picker dialog
#include "apu.h"

// Allows for windows file picker dialog
std::string openFileDialog() {
	char filename[MAX_PATH] = ""; // Stores name of file, max path is default 260

	OPENFILENAMEA ofn; // Configures file picker dialog
	ZeroMemory(&ofn, sizeof(ofn)); // Fills dialog with all 0s to prevent crashing
	ofn.lStructSize = sizeof(ofn); // Windows requires size
	ofn.hwndOwner = nullptr; // Declares that there is no parent window
	ofn.lpstrFilter = "Game Boy ROMs\0*.gb;*.gbc\0All Files\0*.*\0"; // File type filter (type, file extentions shown, secondary filter displayed name, show all files)
	ofn.lpstrFile = filename; // Stores selected file to fileName array
	ofn.nMaxFile = MAX_PATH; // Maximum size of file
	ofn.lpstrTitle = "Select a Game Boy ROM"; // Title bar text
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST; // User can only select files that exist | the folder path must also exist

	// Prevents user from typing a fake path
	if (GetOpenFileNameA(&ofn)) {
		return std::string(filename);
	}
	return ""; // if user cancels
}

int main(int argc, char* argv[]) { // These arguments in main required for SDL2 to link properly

	// Determine ROM path
	std::string romPath;
	if (argc > 1) { // Checks if something was dragged onto the exe or passed via command line
		romPath = argv[1]; // drag and drop
	}
	// If nothing was dragged into the exe. show file picker dialog instead
	else {
		romPath = openFileDialog(); // show file picker
		// If user did not select a rom
		if (romPath.empty()) {
			std::cerr << "No ROM selected\n";
			return 0;
		}
	}

	// Checks to make sure that display/window system was properly initialized and audio is turned on
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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
	if (!bus.loadROM(romPath)) {
		std::cerr << "Failed to load ROM\n";
		return 1;
	}

	// Creates CPU object
	CPU cpu(bus);

	// Creates timer object
	Timer timer(bus);

	// Creates PPU object
	PPU ppu(bus);

	// Creates APU object
	APU apu(bus);
	bus.setAPU(&apu);

	// Creates joypad
	Joypad joypad(bus);
	bus.setJoypad(&joypad);

	// SDL audio setup
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = APU::SAMPLE_RATE;
	want.format = AUDIO_F32SYS;
	want.channels = 2;
	want.samples = APU::BUFFER_SIZE;
	want.callback = [](void* userdata, Uint8* stream, int len) {
		APU* apu = (APU*)userdata;
		apu->fillBuffer((float*)stream, len / sizeof(float));
		};
	want.userdata = &apu;

	SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
	if (audioDevice == 0) {
		std::cerr << "SDL Audio failed: " << SDL_GetError() << "\n";
	}
	SDL_PauseAudioDevice(audioDevice, 0); // start playing

	// Creates texture pointer
	SDL_Texture* texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB24,
		SDL_TEXTUREACCESS_STREAMING,
		160, 144
	);

	bool running = true; // Functions as kill switch for the main loop
	SDL_Event e;		 // Union struct that stores whatever most recently happened (key press, mouse move, window close, etc)

	// Frame timing (60 fps)
	const int TARGET_FPS = 60;
	const int FRAME_TIME_MS = 1000 / TARGET_FPS; // 16ms per frame
	Uint32 frameStart = 0;

	// The main loop, ends when kill switch is set to false
	while (running) {
		frameStart = SDL_GetTicks(); // record start time
		// Pulls one event off SDL's internal queue each call, fills the e struct with it, and returns 1 if there was an event and 0 if queue is empty
		while (SDL_PollEvent(&e)) {
			//If window is closed, call kill switch
			if (e.type == SDL_QUIT) {
				running = false;
			}
			joypad.handleEvent(e); // Handles joypad events
		}

		// Run one frame wortht of cycles (~70224 cycles at 60fps)
		int cycles = 0;
		while (cycles < 70224) {
			try {
				int taken = cpu.step();
				cycles += taken;
				timer.tick(taken); // Tick timer with every cpu step
				apu.tick(taken); // Tick the apu with every cpu step
				for (int i = 0; i < taken; i++) {
					ppu.tick(1);
				}
			}
			catch (const std::runtime_error& e) {
				std::cerr << "CPU ERROR: " << e.what()
					<< " PC=" << std::hex << "\n";
				cycles += 4;
			}
		}
		// After cycle loop, render the frame
		if (ppu.frameReady()) {
			ppu.clearFrameReady();

			// Convert framebuffer to RGB24 format for SDL (3 bytes per pixel)
			std::vector<uint8_t> pixels(160 * 144 * 3);
			for (int i = 0; i < 160 * 144; i++) {
				uint32_t color = ppu.framebuffer[i];
				pixels[i * 3 + 0] = (color >> 16) & 0xFF; // R
				pixels[i * 3 + 1] = (color >> 8) & 0xFF; // G
				pixels[i * 3 + 2] = color & 0xFF; // B
			}

			// Uploads pixel data from ram to gpu texture (texture to update, which region to update (nullptr is all of it), pointer to raw pixel byte array in ram, the pitch (how many bytes per row))
			SDL_UpdateTexture(texture, nullptr, pixels.data(), 160 * 3);
			// Wipes the renderers back buffer with a solid color (resets frame for new frame)
			SDL_RenderClear(renderer);
			// Draws the texture onto the renderers back buffer (renderer to draw into, the texture to draw, which part of source rectangle to use (nullptr is all of it), where to draw destination rectangle (nullptr is filling the entire window))
			SDL_RenderCopy(renderer, texture, nullptr, nullptr);
			// Flips the bakc buffer to the screen
			SDL_RenderPresent(renderer);
		}

		// Cap to 60fps
		Uint32 frameTime = SDL_GetTicks() - frameStart;
		if (frameTime < FRAME_TIME_MS) {
			SDL_Delay(FRAME_TIME_MS - frameTime); // Sleep for remaining time
		}
	}

	// Save External RAM
	bus.saveRAM();

	//Destroy in reverse order of creation
	SDL_CloseAudioDevice(audioDevice);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
