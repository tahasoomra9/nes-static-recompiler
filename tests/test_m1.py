"""Milestone 1 tests using synthetic iNES data only."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "recompiler"))
from decoder import OPCODES, decode
from ines import InesError, parse_ines


class InesTests(unittest.TestCase):
    def test_mapper_zero_header_and_trainer(self):
        header = bytearray(b"NES\x1a\x01\x01\x07\x00" + bytes(8))
        rom = parse_ines(bytes(header) + bytes(512) + bytes(0x4000) + bytes(0x2000))
        self.assertEqual((len(rom.prg_rom), len(rom.chr_rom)), (0x4000, 0x2000))
        self.assertEqual(rom.mirroring, "vertical")
        self.assertTrue(rom.battery)
        self.assertTrue(rom.has_trainer)

    def test_nonzero_mapper_rejected(self):
        header = bytearray(b"NES\x1a\x01\x00\x10\x00" + bytes(8))
        with self.assertRaisesRegex(InesError, "mapper 1 unsupported"):
            parse_ines(bytes(header) + bytes(0x4000))


class DecoderTests(unittest.TestCase):
    def test_complete_official_table(self):
        self.assertEqual(sum(op is not None for op in OPCODES), 151)

    def test_known_sequences(self):
        lda = decode(bytes.fromhex("A9 01"), 0xC000)
        self.assertEqual((lda.opcode.mnemonic, lda.opcode.mode, lda.opcode.bytes), ("LDA", "imm", 2))
        jsr = decode(bytes.fromhex("20 34 C0"), 0x8000)
        self.assertEqual((jsr.opcode.mnemonic, jsr.opcode.mode, jsr.operand), ("JSR", "abs", b"\x34\xc0"))
        branch = decode(bytes.fromhex("D0 FE"), 0xC010)
        self.assertEqual((branch.opcode.mnemonic, branch.opcode.mode, branch.opcode.cycles), ("BNE", "rel", 2))


if __name__ == "__main__":
    unittest.main()
