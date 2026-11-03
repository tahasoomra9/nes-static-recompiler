"""M4 reachable-code discovery tests."""

import unittest

from recompiler.discover import discover
from recompiler.ines import parse_ines


def make_rom(prg: bytes) -> object:
    header = bytes((ord("N"), ord("E"), ord("S"), 0x1A, 1, 0, 0, 0))
    data = header + bytes(8) + prg.ljust(0x4000, b"\xEA")
    return parse_ines(data)


class DiscoveryTests(unittest.TestCase):
    def test_vectors_calls_branches_and_returns(self):
        prg = bytearray(b"\xEA" * 0x4000)
        def put(address, data):
            prg[address - 0xC000:address - 0xC000 + len(data)] = data
        put(0xC000, b"\x20\x10\xC0\xD0\x02\x4C\x20\xC0\x60")
        put(0xC010, b"\xA9\x01\x60")
        put(0xC020, b"\x60")
        prg[0x3FFA:0x4000] = b"\x00\xC0\x00\xC0\x00\xC0"
        result = discover(make_rom(bytes(prg)))
        self.assertIn(0xC000, result.entries)
        self.assertIn(0xC010, result.entries)
        self.assertIn(0xC020, result.entries)
        self.assertGreaterEqual(len(result.instructions), 8)
        self.assertEqual(result.unresolved, ())

    def test_indirect_jump_is_reported(self):
        prg = bytearray(b"\xEA" * 0x4000)
        prg[0:3] = b"\x6C\x00\x80"
        prg[0x3FFA:0x4000] = b"\x00\x80\x00\x80\x00\x80"
        result = discover(make_rom(bytes(prg)))
        self.assertTrue(any(item.kind == "indirect-jmp" for item in result.unresolved))


if __name__ == "__main__":
    unittest.main()
