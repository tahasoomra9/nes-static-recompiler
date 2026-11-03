"""M6 instruction-boundary timing and NMI tests."""

import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


class M6Tests(unittest.TestCase):
    def test_nmi_safe_point_behavior(self):
        result = subprocess.run(
            [str(BUILD / "Release" / "m6_runtime_test.exe")],
            cwd=ROOT, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
