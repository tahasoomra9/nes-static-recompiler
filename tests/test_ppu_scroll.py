"""M3 scanline scroll and raw-frame smoke tests."""

import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WINDOW = ROOT / "build" / "Release" / "nes_window.exe"
ROM = ROOT / "rom-game" / "Super Mario Bros. (Europe).nes"


class PpuScrollTests(unittest.TestCase):
    def test_scroll_log_covers_visible_scanlines(self):
        env = os.environ.copy()
        env["SDL_VIDEODRIVER"] = "dummy"
        result = subprocess.run(
            [str(WINDOW), str(ROM), "--screenshot-frame", "1",
             str(ROOT / "build" / "scroll-test.bmp"), "--scroll-log", "1"],
            capture_output=True,
            text=True,
            timeout=120,
            env=env,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = [line for line in result.stdout.splitlines()
                 if line.startswith("PPU-SCROLL")]
        self.assertEqual(len(lines), 240)
        scanlines = [int(re.search(r"scanline=(\d+)", line).group(1))
                     for line in lines]
        self.assertEqual(scanlines, list(range(240)))
        self.assertTrue(all(" bg=" in line for line in lines))


if __name__ == "__main__":
    unittest.main()
