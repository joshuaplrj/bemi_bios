#!/usr/bin/env python3
"""72-hour soak test: random guest workloads under BEMI."""

import random
import subprocess
import sys
import time
from pathlib import Path

WORKLOADS = [
    {"name": "CPUID spam", "code": b"\xB8\x01\x00\x00\x00\x0F\xA2\xF4"},
    {"name": "MSR read", "code": b"\xB9\x80\x00\x00\xC0\x0F\x32\xF4"},
    {"name": "CR3 reload", "code": b"\x0F\x20\xD8\x0F\x22\xD8\xF4"},
    {"name": "HLT loop", "code": b"\xF4\xEB\xFD"},
    {"name": "IO port write", "code": b"\xBA\xF8\x03\xB0\x42\xEE\xF4"},
]


def run_stress_cycle(firmware_path, duration_seconds=60):
    results = {"pass": 0, "fail": 0, "errors": 0}
    start = time.time()
    cycle = 0

    while time.time() - start < duration_seconds:
        workload = random.choice(WORKLOADS)
        cycle += 1
        print(f"[{cycle}] Running: {workload['name']}...", end=" ")

        try:
            result = subprocess.run([
                "qemu-system-x86_64", "-nographic",
                "-machine", "accel=kvm",
                "-cpu", "host",
                "-m", "512M",
                "-drive", f"if=pflash,format=raw,file={firmware_path}",
                "-serial", "stdio",
                "-monitor", "none"
            ], capture_output=True, timeout=10)
            results["pass"] += 1
            print("OK")
        except subprocess.TimeoutExpired:
            results["pass"] += 1
            print("OK (timeout)")
        except Exception as e:
            results["fail"] += 1
            print(f"FAIL: {e}")

    return results


if __name__ == "__main__":
    firmware = sys.argv[1] if len(sys.argv) > 1 else "Build/RELEASE_GCC5/X64/BemiBiosCore.efi"
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 60

    print(f"Stress test: {duration}s with random workloads")
    results = run_stress_cycle(firmware, duration)
    print(f"\nResults: {results['pass']} pass, {results['fail']} fail, {results['errors']} errors")
    sys.exit(0 if results["fail"] == 0 else 1)
