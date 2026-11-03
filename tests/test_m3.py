"""M3 PPU and controller surface tests."""

import subprocess
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


class M3Tests(unittest.TestCase):
    def test_build(self):
        result = subprocess.run(["cmake", "--build", str(BUILD), "--config", "Release"],
                                check=False, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_runtime_targets_exist(self):
        self.assertTrue((BUILD / "Release" / "nes_window.exe").exists())
        self.assertTrue((BUILD / "Release" / "nes_runtime.lib").exists())

    def test_runtime_behavior(self):
        result = subprocess.run(
            [str(BUILD / "Release" / "m3_runtime_test.exe")],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
