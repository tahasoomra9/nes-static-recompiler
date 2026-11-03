# NES Static Recompiler MVP Specification

## Scope

This project statically recompiles iNES or NES 2.0 NES ROMs to native C.
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

Phase 2 milestones:

1. P0: common illegal opcodes, CHR-RAM, PRG-RAM battery saves, and the mapper
   interface.
2. P1: mapper 2 (UxROM) with bank-aware discovery and emission. UxROM maps
   switchable 16 KiB PRG at `$8000-$BFFF`, fixes the final PRG bank at
   `$C000-$FFFF`, and uses CHR-ROM or writable CHR-RAM.
3. P2: mapper 1 (MMC1) with serial register writes and PRG/CHR banking.
   MMC1 supports control mirroring, all PRG modes, 8 KiB/4 KiB CHR modes,
   PRG-RAM enable, and consecutive-write suppression.
4. P3: APU channels, frame counter, DMC, and SDL2 audio output. The runtime
   advances APU state at CPU-cycle timing points and exposes a 48 kHz signed
   16-bit sample stream through a bounded ring buffer.

Phase 3 milestones:

1. Q0: CPU IRQ line and instruction-boundary interrupt timing.
2. Q1: MMC3 mapper core and bank switching.
3. Q2: MMC3 scanline IRQ from filtered PPU A12 rising edges.
4. Q3: MMC3 bank-aware discovery and emission at 8 KiB granularity.
5. Q4: Playthrough tracing, replayable input, and headless execution.
6. Q5: RAM execution and self-modifying-code detection.
7. Q6: SMB3 bring-up and scripted playthrough.
8. Q7: Bank-aware differential testing and coverage reporting.

The parser reads NES 2.0 mapper and memory metadata. Phase 1 supports mapper 0,
submapper 0, and fixed-size PRG/CHR fields. Phase 2 adds a shared mapper
interface; each mapper validates its own supported sizes and submappers.
Exponent-multiplier ROM sizes remain rejected until explicitly supported.
CHR-RAM is allocated as writable 8 KiB pattern memory when no CHR-ROM is
present. Cartridge PRG-RAM is mapped at `$6000-$7FFF`; battery-backed PRG-RAM
is loaded during ROM initialization and saved beside the user ROM.

Self-modifying code, savestates, GUI tooling, and widescreen output remain out
of scope. P0 supports the commonly used undocumented operations LAX, SAX, DCP,
ISB, SLO, SRE, RLA, RRA, and operand-bearing NOP variants. Other undocumented
operations remain unsupported and must be reported clearly.

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
uint8_t mapper_cpu_read(uint16_t addr);
void mapper_cpu_write(uint16_t addr, uint8_t val);
uint8_t mapper_ppu_read(uint16_t addr);
void mapper_ppu_write(uint16_t addr, uint8_t val);
void mapper_scanline(void);
uint32_t current_prg_bank(uint16_t addr);
void ppu_step(uint32_t cpu_cycles);
void apu_step(uint32_t cpu_cycles);
int16_t apu_sample(void);
void cpu_set_irq_line(uint8_t asserted);
uint8_t cpu_irq_line(void);
bool nmi_pending(void);
void interp_run(uint16_t addr);
void call_addr(uint16_t addr);
```

The mapper interface owns cartridge CPU and PPU mapping. `mapper_scanline()` is
called at scanline timing points, and `current_prg_bank(addr)` identifies the
active code bank for generated dispatch.

## Q0 CPU IRQ behavior

The runtime exposes a level-sensitive `irq_line`. At instruction boundaries,
an asserted line is serviced only when the CPU `I` flag is clear. IRQ pushes
the program counter and processor status with B clear, sets I, adds seven CPU
cycles, and vectors through `$FFFE/$FFFF`. CLI, SEI, PLP, and RTI retain the
6502 one-instruction interrupt latency. Interpreter and recompiled execution
check the same IRQ safe points as NMI: instruction boundaries, I/O accesses,
and loop back-edges.

## Q1 MMC3 mapper behavior

Mapper 4 uses 8 KiB PRG banks and 1 KiB CHR banks. `$8000` selects one of
registers R0-R7 and controls PRG mode and CHR inversion; `$8001` writes the
selected bank. In PRG mode 0, `$8000` is R6 and `$C000` is the second-to-last
bank; in mode 1, `$8000` is the second-to-last bank and `$C000` is R6.
`$A000` selects vertical or horizontal mirroring unless the cartridge is
four-screen, and `$A001` controls PRG-RAM enable/write protection. `$C000`,
`$C001`, `$E000`, and `$E001` hold the MMC3 IRQ latch, reload, acknowledge/
disable, and enable controls for Q2.

R0/R1 select 2 KiB CHR banks with bit 0 ignored; R2-R5 select 1 KiB banks.
CHR inversion swaps the R0/R1 half with the R2-R5 half. PRG-RAM at
`$6000-$7FFF` follows the enable and write-protect bits and uses the existing
battery `.sav` mechanism. Q1 exposes resolved physical PRG/CHR data through
the mapper read APIs; scanline IRQ clocking is deferred to Q2.

## Q2 MMC3 scanline IRQ behavior

The mapper observes PPU A12 transitions while rendering is enabled. A rising
edge clocks the MMC3 counter only after A12 has remained low for at least nine
PPU dots (approximately three CPU cycles). On a clock, the counter loads the
latch when zero or when reload is requested; otherwise it decrements. When the
post-clock value is zero and IRQs are enabled, the CPU `irq_line` is asserted.
`$E000` disables and acknowledges the IRQ; `$E001` enables it. The default is
the newer MMC3 counter behavior.

## P3 APU behavior

The APU maps `$4000-$4017`, including two pulse channels, triangle, noise,
DMC, and the frame counter. Pulse duty/envelope/sweep/length behavior,
triangle linear-counter behavior, noise LFSR modes, DMC sample DMA/IRQ, and
four-step/five-step frame sequencing are represented in the runtime. `$4015`
reports channel activity and DMC status; `$4017` selects the frame sequence
and IRQ inhibit. `apu_step()` is called beside `ppu_step()` at CPU safe points.
SDL audio consumes the runtime's 48 kHz ring buffer; if the buffer is empty it
outputs silence rather than blocking emulation.

## P1 UxROM behavior

Mapper 2 accepts two or more 16 KiB PRG banks. Writes to `$8000-$FFFF` select
the switchable bank; the selected bank is reduced modulo the available bank
count. The final PRG bank is fixed at `$C000-$FFFF`. Generated instruction
wrappers use bank-qualified names (`fn_B_XXXX`) and dispatch through
`current_prg_bank(addr)`. If a bank/address pair is not generated, execution
falls back to one interpreter instruction.

## P2 MMC1 behavior

Mapper 1 accumulates five serial data bits from writes to `$8000-$FFFF`.
Writing a value with bit 7 set resets the shift register and sets the control
register's PRG mode bits to `11`. After five writes, the latched value updates
the target register selected by the address range: control, CHR bank 0, CHR
bank 1, or PRG bank. Control bits select one-screen lower/upper, vertical, or
horizontal mirroring; PRG modes support 32 KiB, fixed-first, and fixed-last
layouts; and CHR mode selects 8 KiB or two 4 KiB banks. PRG-RAM is disabled
when the MMC1 PRG register disable bit is set. Writes on immediately
consecutive CPU cycles are ignored.

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

Use `--trace N` to record the first N instructions as `PC A X Y P SP CYC`.
Use `--trace-file path` to write the trace to a file, and compare interpreter
and recompiled traces with `python tests/compare_trace.py left right`.

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
NMI at the following instruction boundary regardless of the CPU
interrupt-disable flag. Reading `$2002` clears both vblank and a pending NMI; enabling
NMI through `$2000` while vblank is active raises the pending edge.
