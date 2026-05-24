import os
import sys
import subprocess
import glob


def run_tests():
    fail_fast = "--fail-fast" in sys.argv
    test_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tests")
    files = sorted(glob.glob(os.path.join(test_dir, "*.py")))
    files = [f for f in files if not os.path.basename(f).startswith("_")]

    passed = 0
    failed = 0
    skipped = 0

    for path in files:
        name = os.path.basename(path)
        print(f"\n{'=' * 20} Running {name} {'=' * 20}")
        result = subprocess.run([sys.executable, path])
        if result.returncode == 0:
            passed += 1
        else:
            print(f"  [FAIL] {name} exited with code {result.returncode}")
            failed += 1
            if fail_fast:
                break

    print(f"\n{'#' * 60}")
    print(f"  Test Results: {passed} passed, {failed} failed, {skipped} skipped")
    print(f"{'#' * 60}")

    if failed:
        sys.exit(1)


if __name__ == "__main__":
    run_tests()
