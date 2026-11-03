"""Reachable 6502 code discovery for mapper-0 PRG ROMs."""

from dataclasses import dataclass
from pathlib import Path

try:
    from .decoder import DecodedInstruction, decode
    from .ines import InesRom, load_ines
except ImportError:
    from decoder import DecodedInstruction, decode
    from ines import InesRom, load_ines


@dataclass(frozen=True)
class Unresolved:
    address: int
    kind: str
    detail: str


@dataclass(frozen=True)
class Discovery:
    entries: tuple[int, ...]
    instructions: tuple[DecodedInstruction, ...]
    functions: tuple[tuple[int, ...], ...]
    unresolved: tuple[Unresolved, ...]
    prg_bytes: int

    @property
    def coverage_percent(self) -> float:
        covered = sum(instruction.opcode.bytes for instruction in self.instructions)
        return covered * 100.0 / self.prg_bytes if self.prg_bytes else 0.0


def _prg_byte(rom: InesRom, address: int) -> int:
    if not 0x8000 <= address <= 0xFFFF:
        raise ValueError(f"address ${address:04X} is outside PRG space")
    offset = address - 0x8000
    if len(rom.prg_rom) == 0x4000:
        offset &= 0x3FFF
    return rom.prg_rom[offset]


def _read_word(rom: InesRom, address: int) -> int:
    return _prg_byte(rom, address) | (_prg_byte(rom, address + 1) << 8)


def _target(instruction: DecodedInstruction) -> int:
    return instruction.operand[0] | (instruction.operand[1] << 8)


def discover(rom: InesRom, trace_targets: list[int] | None = None) -> Discovery:
    vectors = [_read_word(rom, vector) for vector in (0xFFFA, 0xFFFC, 0xFFFE)]
    entries = set(vector for vector in vectors if 0x8000 <= vector <= 0xFFFF)
    entries.update(trace_targets or [])
    worklist = sorted(entries)
    visited: dict[int, DecodedInstruction] = {}
    functions: list[tuple[int, ...]] = []
    unresolved: list[Unresolved] = []
    queued_entries = set(entries)

    while worklist:
        entry = worklist.pop(0)
        if not 0x8000 <= entry <= 0xFFFF:
            unresolved.append(Unresolved(entry, "entry", "outside PRG address space"))
            continue
        function_addresses: list[int] = []
        address = entry
        local_seen: set[int] = set()
        while address not in local_seen and 0x8000 <= address <= 0xFFFF:
            local_seen.add(address)
            try:
                data = bytes(_prg_byte(rom, address + i) for i in range(3))
                instruction = decode(data, address)
            except (ValueError, IndexError) as error:
                unresolved.append(Unresolved(address, "decode", str(error)))
                break
            visited.setdefault(address, instruction)
            function_addresses.append(address)
            mnemonic = instruction.opcode.mnemonic
            next_address = (address + instruction.opcode.bytes) & 0xFFFF

            if mnemonic in {"JSR", "JMP"} and instruction.opcode.mode == "abs":
                target = _target(instruction)
                if target not in queued_entries:
                    queued_entries.add(target)
                    worklist.append(target)
                if mnemonic == "JMP":
                    break
            elif mnemonic == "JMP" and instruction.opcode.mode == "ind":
                unresolved.append(Unresolved(address, "indirect-jmp", "target is runtime-dependent"))
                break

            if instruction.opcode.mode == "rel":
                displacement = instruction.operand[0]
                if displacement & 0x80:
                    displacement -= 0x100
                target = (next_address + displacement) & 0xFFFF
                if target not in queued_entries:
                    queued_entries.add(target)
                    worklist.append(target)
                if next_address not in queued_entries:
                    queued_entries.add(next_address)
                    worklist.append(next_address)
                break

            if mnemonic in {"RTS", "RTI", "BRK"}:
                break
            address = next_address
        functions.append(tuple(function_addresses))

    return Discovery(
        entries=tuple(sorted(queued_entries)),
        instructions=tuple(sorted(visited.values(), key=lambda item: item.address)),
        functions=tuple(functions),
        unresolved=tuple(unresolved),
        prg_bytes=len(rom.prg_rom),
    )


def discover_path(path: str | Path, trace_targets: list[int] | None = None) -> Discovery:
    return discover(load_ines(path), trace_targets)
