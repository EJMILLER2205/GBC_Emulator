# GBC Emulator

A Game Boy (DMG) emulator written from scratch in C++ using SDL2. Built as a systems programming project to demonstrate low-level hardware emulation, cycle-accurate timing, and audio/video rendering.

![2048 running on the emulator](https://i.imgur.com/placeholder.png)

---

## Features

### CPU
- Full Sharp LR35902 CPU implementation
- All 256 base opcodes plus 256 CB-prefix opcodes
- Accurate flag handling (Zero, Negative, Half Carry, Carry)
- Interrupt system with IME, IE, and IF registers
- HALT instruction with interrupt wake-up
- Passes all 11 Blargg CPU instruction test ROMs

### PPU (Graphics)
- Background, window, and sprite rendering
- Accurate scanline-based rendering with mode timing (OAM scan, Drawing, HBlank, VBlank)
- STAT interrupts and LYC coincidence
- Sprite priority and transparency
- OAM DMA transfers
- VSync support to prevent screen tearing
- 60fps cap compatible with high refresh rate monitors

### APU (Audio)
- All 4 audio channels implemented:
  - Channel 1: Pulse wave with frequency sweep
  - Channel 2: Pulse wave
  - Channel 3: Wave channel (custom waveform from Wave RAM)
  - Channel 4: Noise channel (LFSR-based)
- Volume envelopes and length counters
- Frame sequencer clocked at 512Hz
- Stereo output via SDL2

### Memory
- Full memory bus with accurate address routing
- MBC1 (with secondary bank register and RAM banking modes)
- MBC2 (with internal 512-nibble RAM)
- MBC3 (with RAM/Battery support)
- MBC5 (9-bit ROM banking)
- External RAM (SRAM) with battery save support (.sav files)

### Other
- Windows file picker dialog for ROM selection
- Drag and drop ROM loading
- ROM name displayed in window title bar
- Save file auto-load on startup

---

## Controls

| Game Boy | Keyboard |
|----------|----------|
| A        | Z        |
| B        | X        |
| Start    | Enter    |
| Select   | Backspace|
| D-Pad    | Arrow Keys |

---

## Building

### Requirements
- Visual Studio 2022
- SDL2 (installed via NuGet)
- Windows 10 or later

### Steps
1. Clone the repository
2. Open `GBC_Emulator.sln` in Visual Studio
3. Install SDL2 via NuGet Package Manager
4. Build in Release mode (`Ctrl+Shift+B`)
5. Copy `SDL2.dll` to the output directory

### Running
Either:
- Double click `GBC_Emulator.exe` and use the file picker to select a `.gb` ROM
- Drag and drop a `.gb` file onto `GBC_Emulator.exe`

---

## Test ROMs Passed

| ROM | Result |
|-----|--------|
| 01-special | ✅ Pass |
| 02-interrupts | ✅ Pass |
| 03-op sp,hl | ✅ Pass |
| 04-op r,imm | ✅ Pass |
| 05-op rp | ✅ Pass |
| 06-ld r,r | ✅ Pass |
| 07-jr,jp,call,ret,rst | ✅ Pass |
| 08-misc instrs | ✅ Pass |
| 09-op r,r | ✅ Pass |
| 10-bit ops | ✅ Pass |
| 11-op a,(hl) | ✅ Pass |
| dmg-acid2 | ✅ Mostly pass (minor border inaccuracies) |

---

## Compatibility

Works with most original Game Boy (DMG) ROMs. Tested with:
- 2048.gb
- Tobu Tobu Girl
- Adjustris

### Known Limitations
- Game Boy Color (GBC) games are not supported — color palettes, VRAM banking, and double speed mode are not implemented
- Some mid-frame palette effects may be one frame behind due to scanline-based rendering
- APU audio has some aliasing on high frequency notes (no band limiting)

---

## Architecture

```
main.cpp          — SDL2 setup, main loop, frame timing
cpu.cpp/h         — Sharp LR35902 CPU, all opcodes
bus.cpp/h         — Memory bus, address routing, MBC
ppu.cpp/h         — Pixel Processing Unit, rendering
apu.cpp/h         — Audio Processing Unit, 4 channels
timer.cpp/h       — Hardware timer (DIV, TIMA, TMA, TAC)
joypad.cpp/h      — Joypad input handling
```

### Main Loop
```
For each frame (70224 cycles):
  cpu.step()      → execute one instruction
  timer.tick()    → advance hardware timer
  apu.tick()      → generate audio samples
  ppu.tick(1)     → advance PPU one cycle at a time
```

The PPU is ticked one cycle at a time for accuracy — this ensures LY changes are visible to the CPU at the exact right moment, which is critical for games that poll LY to sync with the display.

---

## References

- [Pan Docs](https://gbdev.io/pandocs/) — Game Boy technical reference
- [rgbds documentation](https://rgbds.gbdev.io/) — instruction timing reference
- [Blargg's test ROMs](https://gbdev.gg/files/roms/) — CPU validation
- [dmg-acid2](https://github.com/mattcurrie/dmg-acid2) — PPU accuracy test

---

## License

This project is for educational purposes. All ROM files are the property of their respective owners and are not included in this repository.
