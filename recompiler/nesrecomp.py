"""Milestone 1 command-line ROM header dumper."""

import argparse
from pathlib import Path

from discover import discover_path
from ines import load_ines
from emit import emit_game


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom")
    parser.add_argument("--discover", action="store_true")
    parser.add_argument("--trace-target", action="append", default=[])
    parser.add_argument("--unresolved", default=None)
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--out", default="build")
    args = parser.parse_args()
    rom_path = Path(args.rom)
    if not rom_path.is_file():
        parser.error(
            f"ROM not found: {rom_path}. Pass a ROM path, for example "
            "rom-game\\Super Mario Bros. (Europe).nes"
        )
    try:
        rom = load_ines(rom_path)
    except OSError as error:
        parser.error(f"cannot read ROM {rom_path}: {error}")
    print(f"Format: {rom.format}")
    print(f"Mapper: {rom.mapper}")
    print(f"Submapper: {rom.submapper}")
    print(f"PRG-ROM: {len(rom.prg_rom) // 1024} KB")
    print(f"CHR-ROM: {len(rom.chr_rom) // 1024} KB")
    print(f"Mirroring: {rom.mirroring}")
    print(f"Battery: {'yes' if rom.battery else 'no'}")
    print(f"Trainer: {'yes' if rom.has_trainer else 'no'}")
    print(f"PRG-RAM: {rom.prg_ram_size // 1024} KB")
    print(f"CHR-RAM: {rom.chr_ram_size // 1024} KB")
    print(f"Timing: {rom.timing}")
    if args.discover or args.emit:
        targets = [int(value, 0) for value in args.trace_target]
        trace_file = Path(args.out) / "trace_targets.txt"
        if trace_file.exists():
            targets.extend(
                int(line.strip(), 0) for line in trace_file.read_text().splitlines()
                if line.strip() and not line.lstrip().startswith("#")
            )
        result = discover_path(args.rom, targets)
        print(f"Discovered functions: {len(result.functions)}")
        print(f"Discovered instructions: {len(result.instructions)}")
        print(f"PRG coverage: {result.coverage_percent:.2f}%")
        print(f"Unresolved: {len(result.unresolved)}")
        for item in result.unresolved:
            print(f"${item.address:04X} {item.kind}: {item.detail}")
        if args.unresolved:
            with open(args.unresolved, "w", encoding="ascii") as output:
                for item in result.unresolved:
                    output.write(f"${item.address:04X} {item.kind}: {item.detail}\n")
        if args.emit:
            output = Path(args.out)
            output.mkdir(parents=True, exist_ok=True)
            emit_game(result, output / "game.c")
            print(f"Generated: {output / 'game.c'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
