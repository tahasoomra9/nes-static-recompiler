"""Parser for iNES and NES 2.0 ROM headers."""

from dataclasses import dataclass
from pathlib import Path
import warnings


class InesError(ValueError):
    """Raised when a ROM is malformed or outside the MVP scope."""


@dataclass(frozen=True)
class InesRom:
    format: str
    prg_rom: bytes
    chr_rom: bytes
    chr_ram: bytearray
    mapper: int
    submapper: int
    mirroring: str
    battery: bool
    has_trainer: bool
    four_screen: bool
    prg_ram_size: int
    chr_ram_size: int
    timing: str

    @property
    def prg_banks(self) -> int:
        return len(self.prg_rom) // 0x4000

    @property
    def chr_banks(self) -> int:
        return len(self.chr_rom) // 0x2000


def _shifted_ram_size(value: int) -> int:
    return 0 if value == 0 else 64 << value


def _nes2_rom_size(low: int, high_nibble: int, label: str) -> int:
    if high_nibble == 0x0F:
        exponent = low >> 2
        multiplier = low & 0x03
        raise InesError(
            f"{label} uses unsupported exponent-multiplier form "
            f"(exponent {exponent}, multiplier {multiplier})"
        )
    return ((high_nibble << 8) | low) * (0x4000 if label == "PRG-ROM" else 0x2000)


def parse_ines(data: bytes) -> InesRom:
    if len(data) < 16 or data[:4] != b"NES\x1a":
        raise InesError(r"invalid iNES header: expected NES\1a signature")

    prg_low, chr_low = data[4], data[5]
    flags6, flags7 = data[6], data[7]
    nes2 = (flags7 & 0x0C) == 0x08
    format_name = "NES 2.0" if nes2 else "iNES 1.0"
    if nes2:
        mapper = (flags6 >> 4) | (flags7 & 0xF0) | ((data[8] & 0x0F) << 8)
        submapper = data[8] >> 4
        prg_size = _nes2_rom_size(prg_low, data[9] & 0x0F, "PRG-ROM")
        chr_size = _nes2_rom_size(chr_low, data[9] >> 4, "CHR-ROM")
        prg_ram_size = _shifted_ram_size(data[10] & 0x0F)
        prg_ram_size += _shifted_ram_size(data[10] >> 4)
        chr_ram_size = _shifted_ram_size(data[11] & 0x0F)
        chr_ram_size += _shifted_ram_size(data[11] >> 4)
        timing = ("NTSC", "PAL", "multiple-region", "Dendy")[data[12] & 0x03]
    else:
        mapper = (flags6 >> 4) | (flags7 & 0xF0)
        submapper = 0
        prg_size = prg_low * 0x4000
        chr_size = chr_low * 0x2000
        prg_ram_size = 0
        chr_ram_size = 0x2000 if chr_size == 0 else 0
        timing = "NTSC"

    if mapper not in (0, 1, 2, 4):
        raise InesError(f"mapper {mapper} unsupported; supported mappers are 0, 1, 2, and 4")
    if submapper != 0:
        raise InesError(f"submapper {submapper} unsupported; only submapper 0 is supported")
    if mapper == 0 and not nes2 and prg_low not in (1, 2):
        raise InesError(f"mapper 0 requires 1 or 2 PRG banks, got {prg_low}")
    if mapper == 2 and prg_low < 2:
        raise InesError(f"mapper 2 requires at least 2 PRG banks, got {prg_low}")
    if mapper == 1 and prg_low < 2:
        raise InesError(f"mapper 1 requires at least 2 PRG banks, got {prg_low}")
    if mapper == 4 and prg_size < 0x8000:
        raise InesError("mapper 4 requires at least 32 KiB of PRG-ROM")

    has_trainer = bool(flags6 & 0x04)
    offset = 16 + (512 if has_trainer else 0)
    end = offset + prg_size + chr_size
    if len(data) < end:
        raise InesError(f"truncated ROM: expected {end} bytes, got {len(data)}")
    if len(data) > end:
        warnings.warn(f"ROM has {len(data) - end} trailing bytes", RuntimeWarning)

    four_screen = bool(flags6 & 0x08)
    mirroring = "four-screen" if four_screen else ("vertical" if flags6 & 1 else "horizontal")
    chr_rom = data[offset + prg_size:end]
    return InesRom(
        format=format_name,
        prg_rom=data[offset:offset + prg_size],
        chr_rom=chr_rom,
        chr_ram=bytearray(chr_ram_size if not chr_rom and chr_ram_size else 0),
        mapper=mapper,
        submapper=submapper,
        mirroring=mirroring,
        battery=bool(flags6 & 0x02),
        has_trainer=has_trainer,
        four_screen=four_screen,
        prg_ram_size=prg_ram_size,
        chr_ram_size=chr_ram_size,
        timing=timing,
    )


def load_ines(path: str | Path) -> InesRom:
    return parse_ines(Path(path).read_bytes())
