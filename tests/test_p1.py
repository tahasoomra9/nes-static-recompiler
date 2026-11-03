"""Phase 2 P1 UxROM tests."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "recompiler"))
from decoder import decode
from ines import parse_ines


class UxromHeaderTests(unittest.TestCase):
    def test_mapper_two_accepts_multiple_prg_banks(self):
        header = bytearray(b"NES\x1a\x02\x00\x20\x00" + bytes(8))
        rom = parse_ines(bytes(header) + bytes(0x8000))
        self.assertEqual((rom.mapper, rom.prg_banks), (2, 2))

    def test_mapper_two_requires_at_least_two_banks(self):
        header = bytearray(b"NES\x1a\x01\x00\x20\x00" + bytes(8))
        with self.assertRaisesRegex(ValueError, "mapper 2"):
            parse_ines(bytes(header) + bytes(0x4000))


class UxromEmissionTests(unittest.TestCase):
    def test_discovery_keeps_switchable_banks_separate(self):
        from discover import discover

        header = b"NES\x1a\x02\x00\x20\x00" + bytes(8)
        prg = bytearray(0x8000)
        for offset in (0x0000, 0x4000):
            prg[offset:offset + 2] = b"\xEA\x60"
        prg[0x7FFC:0x8000] = b"\x00\xC0\x00\xC0"
        rom = parse_ines(header + bytes(prg))
        result = discover(rom)
        self.assertIn((0, 0x8000), {(item.bank, item.address) for item in result.instructions})
        self.assertIn((1, 0x8000), {(item.bank, item.address) for item in result.instructions})

    def test_bank_aware_function_name(self):
        from discover import Discovery
        from emit import emit_game
        from tempfile import TemporaryDirectory

        instruction = decode(bytes.fromhex("EA"), 0x8000)
        discovery = Discovery(
            entries=(0x8000,), instructions=(instruction,),
            functions=((0x8000,),), unresolved=(), prg_bytes=0x8000,
        )
        with TemporaryDirectory() as directory:
            output = Path(directory) / "game.c"
            emit_game(discovery, output)
            source = output.read_text()
        self.assertIn("fn_B_8000", source)
        self.assertIn("current_prg_bank(addr)", source)
