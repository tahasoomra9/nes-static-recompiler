"""Phase 2 P2 MMC1 tests."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "recompiler"))
from ines import parse_ines


class Mmc1HeaderTests(unittest.TestCase):
    def test_mapper_one_is_accepted(self):
        header = b"NES\x1a\x04\x02\x10\x00" + bytes(8)
        rom = parse_ines(header + bytes(0x10000) + bytes(0x4000))
        self.assertEqual((rom.mapper, rom.prg_banks, rom.chr_banks), (1, 4, 2))


if __name__ == "__main__":
    unittest.main()
