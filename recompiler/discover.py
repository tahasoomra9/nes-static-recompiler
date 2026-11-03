"""Reachable 6502 code discovery for mapper-0 PRG ROMs."""

from dataclasses import dataclass, replace
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


def _prg_byte(rom: InesRom, address: int, bank: int = 0) -> int:
    if not 0x8000 <= address <= 0xFFFF:
        raise ValueError(f"address ${address:04X} is outside PRG space")
    if rom.mapper == 2:
        bank_count = len(rom.prg_rom) // 0x4000
        if address >= 0xC000:
            offset = (bank_count - 1) * 0x4000 + address - 0xC000
        else:
            offset = (bank % bank_count) * 0x4000 + address - 0x8000
    else:
        offset = address - 0x8000
        if len(rom.prg_rom) == 0x4000:
            offset &= 0x3FFF
    return rom.prg_rom[offset]


def _read_word(rom: InesRom, address: int, bank: int = 0) -> int:
    return _prg_byte(rom, address, bank) | (_prg_byte(rom, address + 1, bank) << 8)


def _target(instruction: DecodedInstruction) -> int:
    return instruction.operand[0] | (instruction.operand[1] << 8)


def discover(rom: InesRom, trace_targets: list[int] | None = None) -> Discovery:
    fixed_bank = 0 if rom.mapper == 0 else max(0, rom.prg_banks - 1)
    vector_entries = [
        (fixed_bank, _read_word(rom, vector, fixed_bank))
        for vector in (0xFFFA, 0xFFFC, 0xFFFE)
    ]
    entries = set(
        (bank, vector) for bank, vector in vector_entries
        if 0x8000 <= vector <= 0xFFFF
    )
    if rom.mapper == 2:
        entries.update((bank, 0x8000) for bank in range(rom.prg_banks))
    entries.update((0, target) for target in (trace_targets or []))
    worklist = sorted(entries)
    visited: dict[tuple[int, int], DecodedInstruction] = {}
    functions: list[tuple[int, ...]] = []
    unresolved: list[Unresolved] = []
    queued_entries = set(entries)

    while worklist:
        bank, entry = worklist.pop(0)
        if not 0x8000 <= entry <= 0xFFFF:
            unresolved.append(Unresolved(entry, "entry", "outside PRG address space"))
            continue
        function_addresses: list[int] = []
        address = entry
        local_seen: set[int] = set()
        while address not in local_seen and 0x8000 <= address <= 0xFFFF:
            local_seen.add(address)
            try:
                data = bytes(_prg_byte(rom, address + i, bank) for i in range(3))
                instruction = decode(data, address)
                instruction = replace(instruction, bank=bank)
            except (ValueError, IndexError) as error:
                unresolved.append(Unresolved(address, "decode", str(error)))
                break
            visited.setdefault((bank, address), instruction)
            function_addresses.append(address)
            mnemonic = instruction.opcode.mnemonic
            next_address = (address + instruction.opcode.bytes) & 0xFFFF

            if mnemonic in {"JSR", "JMP"} and instruction.opcode.mode == "abs":
                target = _target(instruction)
                target_bank = fixed_bank if target >= 0xC000 else bank
                target_entry = (target_bank, target)
                if target_entry not in queued_entries:
                    queued_entries.add(target_entry)
                    worklist.append(target_entry)
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
                target_entry = (fixed_bank if target >= 0xC000 else bank, target)
                fallthrough_entry = (
                    fixed_bank if next_address >= 0xC000 else bank, next_address
                )
                if target_entry not in queued_entries:
                    queued_entries.add(target_entry)
                    worklist.append(target_entry)
                if fallthrough_entry not in queued_entries:
                    queued_entries.add(fallthrough_entry)
                    worklist.append(fallthrough_entry)
                break

            if mnemonic in {"RTS", "RTI", "BRK"}:
                break
            address = next_address
        functions.append(tuple(function_addresses))

    return Discovery(
        entries=tuple(sorted(address for _, address in queued_entries)),
        instructions=tuple(sorted(visited.values(), key=lambda item: item.address)),
        functions=tuple(functions),
        unresolved=tuple(unresolved),
        prg_bytes=len(rom.prg_rom),
    )


def discover_path(path: str | Path, trace_targets: list[int] | None = None) -> Discovery:
    return discover(load_ines(path), trace_targets)
