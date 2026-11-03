"""Compare two runtime traces and report the first divergent instruction."""

from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: compare_trace.py interpreter.trace recompiled.trace")
        return 2
    left = Path(sys.argv[1]).read_text().splitlines()
    right = Path(sys.argv[2]).read_text().splitlines()
    for index, (expected, actual) in enumerate(zip(left, right), 1):
        if expected != actual:
            print(f"first divergence at instruction {index}")
            print(f"interpreter: {expected}")
            print(f"recompiled:  {actual}")
            return 1
    if len(left) != len(right):
        print(f"trace length differs: interpreter={len(left)} recompiled={len(right)}")
        return 1
    print(f"traces match: {len(left)} instructions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
