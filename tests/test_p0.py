"""Phase 2 P0 regression tests."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "recompiler"))
from decoder import OPCODES, decode
from ines import parse_ines


class P0DecoderTests(unittest.TestCase):
    def test_common_unofficial_opcodes_have_metadata(self):
        expected = {
            0xA7: ("LAX", "zp", 2, 3),
            0x87: ("SAX", "zp", 2, 3),
            0xC7: ("DCP", "zp", 2, 5),
            0xE7: ("ISB", "zp", 2, 5),
            0x07: ("SLO", "zp", 2, 5),
            0x47: ("SRE", "zp", 2, 5),
            0x27: ("RLA", "zp", 2, 5),
            0x67: ("RRA", "zp", 2, 5),
            0x80: ("NOP", "imm", 2, 2),
            0x0C: ("NOP", "abs", 3, 4),
        }
        for code, metadata in expected.items():
            op = OPCODES[code]
            self.assertIsNotNone(op, f"${code:02X} missing")
            self.assertEqual((op.mnemonic, op.mode, op.bytes, op.cycles), metadata)

    def test_nop_operands_are_consumed(self):
        decoded = decode(bytes.fromhex("1C 34 12"), 0xC000)
        self.assertEqual(
            (decoded.opcode.mnemonic, decoded.opcode.bytes, decoded.operand),
            ("NOP", 3, b"\x34\x12"),
        )


class P0CartridgeTests(unittest.TestCase):
    def test_no_chr_rom_allocates_writable_chr_ram(self):
        header = b"NES\x1a\x01\x00\x02\x00" + bytes(8)
        rom = parse_ines(header + bytes(0x4000))
        self.assertEqual(len(rom.chr_rom), 0)
        self.assertEqual(len(rom.chr_ram), 0x2000)
        self.assertEqual(rom.prg_ram_size, 0)

    def test_ines_battery_declares_prg_ram(self):
        header = b"NES\x1a\x01\x00\x02\x00" + bytes(8)
        rom = parse_ines(header + bytes(0x4000))
        self.assertTrue(rom.battery)


if __name__ == "__main__":
    unittest.main()
