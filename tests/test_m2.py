"""M2 runtime build and basic CPU smoke test."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


class RuntimeTests(unittest.TestCase):
    def test_runner_build_exists(self):
        cmake = shutil.which("cmake")
        if cmake is None:
            self.skipTest("cmake unavailable")
        result = subprocess.run(
            [cmake, "--build", str(BUILD), "--config", "Release", "--target", "nestest_runner"],
            check=False,
        )
        self.assertEqual(result.returncode, 0)

    def test_synthetic_reset_trace(self):
        runner = BUILD / "Release" / "nestest_runner.exe"
        if not runner.exists():
            self.skipTest("build nestest_runner first")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "synthetic.nes"
            header = b"NES\x1a" + bytes([2, 0, 0, 0]) + bytes(8)
            prg = bytearray(0x8000)
            prg[0x4000] = 0xa9
            prg[0x4001] = 0x42
            prg[0x4002] = 0xea
            path.write_bytes(header + prg)
            output = subprocess.run(
                [str(runner), str(path)],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(output.returncode, 0)
            self.assertIn("A:00", output.stdout.splitlines()[0])
            self.assertIn("A:42", output.stdout.splitlines()[1])


if __name__ == "__main__":
    unittest.main()
