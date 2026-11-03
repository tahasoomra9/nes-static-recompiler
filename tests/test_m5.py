"""M5 generated-C and build integration tests."""

import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
ROM = ROOT / "tests" / "nestest" / "nestest.nes"


class EmitterTests(unittest.TestCase):
    def test_emit_builds_game_target(self):
        result = subprocess.run(
            [sys.executable, "recompiler\\nesrecomp.py", str(ROM),
             "--emit", "--out", str(BUILD)],
            cwd=ROOT, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        source = BUILD / "game.c"
        self.assertTrue(source.exists())
        text = source.read_text(encoding="ascii")
        self.assertIn("fn_", text)
        self.assertIn("generated_dispatch", text)
        self.assertIn("game_main", text)

        result = subprocess.run(
            ["cmake", "--build", str(BUILD), "--config", "Release", "--target", "game"],
            cwd=ROOT, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((BUILD / "Release" / "game.exe").exists())


if __name__ == "__main__":
    unittest.main()
