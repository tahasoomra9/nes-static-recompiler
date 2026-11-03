"""Phase 3 Q1 MMC3 parser coverage."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "recompiler"))
from ines import parse_ines


class Mmc3HeaderTests(unittest.TestCase):
    def test_mapper_four_is_accepted(self):
        header = b"NES\x1a\x08\x04\x40\x00" + bytes(8)
        rom = parse_ines(header + bytes(0x20000) + bytes(0x8000))
        self.assertEqual((rom.mapper, rom.prg_banks, rom.chr_banks), (4, 8, 4))

    def test_mapper_four_requires_two_prg_banks(self):
        header = b"NES\x1a\x01\x01\x40\x00" + bytes(8)
        with self.assertRaises(ValueError):
            parse_ines(header + bytes(0x4000) + bytes(0x2000))


if __name__ == "__main__":
    unittest.main()
