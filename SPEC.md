# NES Static Recompiler MVP Specification

## Scope

This project statically recompiles mapper 0 (NROM) iNES or NES 2.0 NES ROMs to native C.
The input ROM is supplied by the user and is never embedded in the repository or
generated source. The runtime is C11 and the recompiler is Python 3.10+ using
only the standard library.

Milestones are completed in order:

1. iNES parsing and the complete official 6502 opcode table.
2. Runtime CPU, mapper 0 bus, and interpreter fallback. The runtime exposes
   `nestest_runner` and `tests/run_nestest.py`; the supplied nestest ROM/log are
   required for the line-by-line acceptance test.
3. PPU, controller input, and SDL2 presentation.
4. Reachable-code discovery.
5. C lifting, emission, and generated CMake build.
6. Timing and correctness validation.
7. Differential tracing.

The parser reads NES 2.0 mapper and memory metadata. Mapper 0, submapper 0,
and fixed-size PRG/CHR fields are supported; exponent-multiplier ROM sizes are
rejected because no NROM game uses them. CHR-RAM is writable when no CHR-ROM
is present.

Out of scope are APU/audio, non-zero mappers, savestates, unsupported illegal
opcodes, self-modifying code, GUI tooling, and widescreen output.

## Repository layout

The source directories are `recompiler/`, `runtime/`, and `tests/`. Generated
files belong under `build/`, which is ignored by version control.

## Runtime API contract

The following declarations and semantics are stable across all milestones:

```c
typedef struct {
    uint8_t a, x, y, sp;
    uint16_t pc;
    uint8_t n, v, d, i, z, c;
    uint64_t cycles;
} Cpu;

extern Cpu cpu;
uint8_t bus_read(uint16_t addr);
void bus_write(uint16_t addr, uint8_t val);
void ppu_step(uint32_t cpu_cycles);
bool nmi_pending(void);
void interp_run(uint16_t addr);
void call_addr(uint16_t addr);
```

Generated code contains one `fn_XXXX(void)` function per discovered
subroutine, a `dispatch(uint16_t addr)` switch, and `game_main()` starting at
the reset vector. Every instruction accounts for its documented CPU cycles.
JSR and RTS use the real 6502 stack at `$0100 + SP`; all memory access goes
through `bus_read` and `bus_write`.

## Build

On Windows, install CMake, a C11 compiler, and SDL2. A vcpkg-configured
generator can be used as follows:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
```

The M0 executable opens a 256x240 SDL2 window scaled by two. Later milestones
add the NES runtime and generated game code.

## M4 code discovery

`python recompiler/nesrecomp.py game.nes --discover --unresolved
build/unresolved.txt` starts from the RESET, NMI, and IRQ vectors, follows
direct JSR/JMP targets and conditional branch targets, and records discovered
6502 instructions grouped by entry point. Indirect JMPs and decode failures
are reported as unresolved; optional `--trace-target 0xXXXX` arguments add
extra entry points.

The ROM argument must point to an existing user-supplied ROM; `game.nes` is
only an example name and is not included in the repository.

Runtime diagnostics are opt-in. Use `--frame-log` to print per-frame PPU
diagnostics and `--scroll-log N` to print the selected frame's scroll data.

`run_game.bat [rom-path]` launches any mapper-0 `.nes` ROM with the interpreter.
Pass `--recompiled` as the second argument only for a ROM matching the generated
`build/game.c`; generated code is not interchangeable between ROMs.

## M3 runtime behavior

The mapper-0 runtime provides PPU register access with nametable mirroring,
palette aliases, scroll registers, CHR-ROM/CHR-RAM pattern access, background
attributes, 8x8/8x16 sprites, sprite priority/flips, sprite-0 hit status, NMI
generation, and controller shift-register input. `nes_window` maps keyboard
input to controller 1 (`Z`/`X`, Right Shift, Enter, and arrow keys).

For deterministic capture, use:

```powershell
build\Release\nes_window.exe game.nes --screenshot-frame 1 build\frame.bmp
```

Screenshot mode exits after saving the requested frame. The renderer is still
an MVP approximation and does not include audio or non-zero mappers.

## M6 timing behavior

The runtime advances the PPU after each CPU instruction and services a pending
NMI only at the following instruction boundary when the CPU interrupt-disable
flag is clear. Reading `$2002` clears both vblank and a pending NMI; enabling
NMI through `$2000` while vblank is active raises the pending edge.
