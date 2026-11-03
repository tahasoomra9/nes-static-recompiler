"""Focused iNES and NES 2.0 parser tests using in-memory ROMs."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "recompiler"))
from ines import InesError, parse_ines


def rom(header: bytes, trainer: bool = False, prg: int = 0x8000, chr_size: int = 0x2000) -> bytes:
    return header + (bytes(512) if trainer else b"") + bytes(prg + chr_size)


class InesFormatTests(unittest.TestCase):
    def test_ines_1_mapper_zero(self):
        header = b"NES\x1a" + bytes([2, 1, 0, 0]) + bytes(8)
        parsed = parse_ines(rom(header))
        self.assertEqual((parsed.format, parsed.mapper, len(parsed.prg_rom), len(parsed.chr_rom)),
                         ("iNES 1.0", 0, 0x8000, 0x2000))

    def test_nes_2_mapper_zero(self):
        header = b"NES\x1a" + bytes([2, 1, 0, 8]) + bytes(8)
        parsed = parse_ines(rom(header))
        self.assertEqual((parsed.format, parsed.mapper, parsed.submapper), ("NES 2.0", 0, 0))

    def test_nes_2_mapper_rejected(self):
        header = b"NES\x1a" + bytes([2, 1, 0x40, 8]) + bytes(8)
        with self.assertRaisesRegex(InesError, "mapper 4 unsupported"):
            parse_ines(rom(header))

    def test_nes_2_trainer_shifts_prg(self):
        header = b"NES\x1a" + bytes([1, 0, 4, 8]) + bytes(8)
        payload = header + bytes(512) + bytes([0xA5]) + bytes(0x3FFF)
        self.assertEqual(parse_ines(payload).prg_rom[0], 0xA5)

    def test_exponent_form_rejected(self):
        header = b"NES\x1a" + bytes([0x0F, 0, 0, 8, 0, 0x0F]) + bytes(6)
        with self.assertRaisesRegex(InesError, "exponent-multiplier"):
            parse_ines(header)

    def test_truncated_file_rejected(self):
        header = b"NES\x1a" + bytes([2, 1, 0, 8]) + bytes(8)
        with self.assertRaisesRegex(InesError, "truncated ROM"):
            parse_ines(header + bytes(100))


if __name__ == "__main__":
    unittest.main()
