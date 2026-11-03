"""M0 smoke test: configure and build the SDL2 window."""

from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def main() -> int:
    cmake = shutil.which("cmake")
    if cmake is None:
        print("FAIL: cmake is not installed or not on PATH", file=sys.stderr)
        return 2

    configure = subprocess.run(
        [cmake, "-S", str(ROOT), "-B", str(BUILD)],
        check=False,
    )
    if configure.returncode != 0:
        return configure.returncode

    build = subprocess.run(
        [cmake, "--build", str(BUILD), "--config", "Release"],
        check=False,
    )
    return build.returncode


if __name__ == "__main__":
    raise SystemExit(main())
