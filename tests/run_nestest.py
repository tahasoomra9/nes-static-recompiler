"""Run the native trace runner and compare it with a supplied nestest log."""

from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / "tests" / "nestest" / "nestest.nes"
LOG = ROOT / "tests" / "nestest" / "nestest.log"
RUNNER = ROOT / "build" / "Release" / "nestest_runner.exe"


def main() -> int:
    missing = [str(path) for path in (ROM, LOG, RUNNER) if not path.exists()]
    if missing:
        print("nestest assets missing: " + ", ".join(missing), file=sys.stderr)
        return 2
    expected = []
    for line in LOG.read_text(errors="replace").splitlines():
        match = re.match(
            r"^\s*([0-9A-F]{4}).*A:([0-9A-F]{2}).*X:([0-9A-F]{2}).*"
            r"Y:([0-9A-F]{2}).*P:([0-9A-F]{2}).*SP:([0-9A-F]{2}).*CYC:(\d+)",
            line,
        )
        if match:
            expected.append(match.groups())
    actual = subprocess.run(
        [str(RUNNER), str(ROM)], capture_output=True, text=True, check=False
    )
    if actual.returncode != 0:
        return actual.returncode
    pattern = re.compile(
        r"^([0-9A-F]{4}) A:([0-9A-F]{2}) X:([0-9A-F]{2}) Y:([0-9A-F]{2}) "
        r"P:([0-9A-F]{2}) SP:([0-9A-F]{2}) CYC:(\d+)$"
    )
    for index, (want, line) in enumerate(zip(expected, actual.stdout.splitlines())):
        match = pattern.match(line)
        got = match.groups() if match else ()
        if got != want:
            print(f"first divergence at line {index + 1}: expected {want}, got {got}")
            print(f"trace: {line}")
            return 1
    print(f"matched {min(len(expected), len(actual.stdout.splitlines()))} trace lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
