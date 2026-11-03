"""M7 trace capture and comparison tests."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class M7Tests(unittest.TestCase):
    def test_compare_trace_reports_match(self):
        with tempfile.TemporaryDirectory() as directory:
            left = Path(directory) / "left.trace"
            right = Path(directory) / "right.trace"
            left.write_text("8000 A:00 X:00 Y:00 P:24 SP:FD CYC:2\n")
            right.write_text(left.read_text())
            result = subprocess.run(
                [sys.executable, "tests\\compare_trace.py", str(left), str(right)],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("traces match: 1 instructions", result.stdout)


if __name__ == "__main__":
    unittest.main()
