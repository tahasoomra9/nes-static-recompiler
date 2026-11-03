"""Phase 2 P3 APU tests."""

import unittest


class ApuContractTests(unittest.TestCase):
    def test_public_runtime_target_exists(self):
        from pathlib import Path
        root = Path(__file__).parents[1]
        self.assertTrue((root / "runtime" / "apu.h").is_file())
        self.assertIn("apu_step", (root / "SPEC.md").read_text())


if __name__ == "__main__":
    unittest.main()
