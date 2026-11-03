"""Background tile-address regression checks."""

import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WINDOW = ROOT / "build" / "Release" / "nes_window.exe"
ROM = ROOT / "rom-game" / "Super Mario Bros. (Europe).nes"


class BackgroundTileTests(unittest.TestCase):
    def test_coarse_x_advances_once_per_tile(self):
        env = os.environ.copy()
        env["SDL_VIDEODRIVER"] = "dummy"
        result = subprocess.run(
            [str(WINDOW), str(ROM), "--screenshot-frame", "1",
             str(ROOT / "build" / "bg-test.bmp"), "--bg-log"],
            capture_output=True,
            text=True,
            timeout=120,
            env=env,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = [line for line in result.stdout.splitlines()
                 if line.startswith("PPU-BG")][:32]
        self.assertEqual(len(lines), 32)
        coarse = [int(re.search(r"coarse_x=(\d+)", line).group(1))
                  for line in lines]
        addresses = [int(re.search(r"nt=([0-9A-F]+)", line).group(1), 16)
                     for line in lines]
        self.assertEqual(coarse, list(range(32)))
        self.assertEqual(addresses, list(range(0x2000, 0x2020)))


if __name__ == "__main__":
    unittest.main()
