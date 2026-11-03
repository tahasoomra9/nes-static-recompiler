# NES Static Recompiler

A Windows NES runtime and static recompiler MVP for mapper 0/NROM games.

## Features

- iNES and NES 2.0 parsing
- 6502 interpreter and code discovery
- Generated C code for discovered routines
- PPU background, sprites, scrolling, NMI, and controller input
- SDL2 256x240 display
- Screenshot and PPM capture
- nestest validation

## Requirements

- Windows
- Python 3.10+
- CMake 3.20+
- Visual Studio/MSVC or Clang
- SDL2 (CMake FetchContent can download it)

## Build

From the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run any supported ROM

Put a mapper-0 or mapper-2 `.nes` ROM in `rom-game\`, then run:

```powershell
.\run_game.bat "Duck Hunt (World).nes"
```

A bare filename is resolved inside `rom-game`. With no argument, the first
`.nes` file in that folder is selected:

```powershell
.\run_game.bat
```

The interpreter runtime is used for generic ROMs.

## Generate and run recompiled code

Generated code is ROM-specific:

```powershell
$env:PYTHONPATH='.'
python recompiler\nesrecomp.py `
  "rom-game\Super Mario Bros. (Europe).nes" `
  --emit --out build

cmake --build build --config Release --target game
build\Release\game.exe `
  "rom-game\Super Mario Bros. (Europe).nes" `
  --recompiled
```

Do not use `--recompiled` with a different ROM.

## Controls

- Arrow keys: D-pad
- `Z`: B
- `X`: A
- `Enter`: Start
- Right Shift: Select

## Screenshots

```powershell
build\Release\nes_window.exe `
  "rom-game\Super Mario Bros. (Europe).nes" `
  --screenshot-frame 120 `
  build\frame120.bmp
```

Trace the first N instructions:

```powershell
build\Release\nes_window.exe `
  "rom-game\Super Mario Bros. (Europe).nes" `
  --trace 100 `
  --trace-file build\interpreter.trace
```

Use `--recompiled --trace N` with the generated executable to capture the
recompiled path.

The runtime also generates SDL2 audio at 48 kHz for the two pulse channels,
triangle, noise, DMC, and frame-counter timing.

Phase 3 Q0 adds a level-sensitive CPU IRQ line. IRQs use the `$FFFE` vector,
push status with B clear, and preserve the 6502 one-instruction latency after
CLI, SEI, PLP, and RTI.

The MMC3 core now observes filtered PPU A12 rising edges while rendering and
drives its latch/reload IRQ counter through the CPU IRQ line. Dedicated MMC3
scanline test ROM validation still requires user-supplied Blargg ROMs.

## Tests

```powershell
$env:PYTHONPATH='.'
python -m unittest discover -s tests -p 'test_*.py'
python tests\run_nestest.py
```

## Limitations

Mapper 0/NROM, mapper 2/UxROM, mapper 1/MMC1, and the MMC3 mapper core are
supported. UxROM provides switchable 16 KiB PRG banks, a fixed final bank, and
bank-aware generated dispatch. MMC1 provides serial register writes, PRG/CHR
banking, and mirroring control. MMC3 provides 8 KiB PRG banking, 1 KiB CHR
banking, mirroring, protected PRG-RAM, and IRQ register state. Other mappers,
savestates,
self-modifying code and unsupported undocumented opcodes remain outside the
current scope. P0 supports common unofficial 6502 operations and NOP variants,
CHR-RAM, 8 KiB PRG-RAM, and optional battery `.sav` persistence. Additional
APU accuracy remains under validation against dedicated hardware test ROMs.
Generated output and screenshots belong under `build\`.
