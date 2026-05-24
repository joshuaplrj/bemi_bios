#!/usr/bin/env python3
"""BEMI BIOS QEMU test harness — Phase 7 validation"""

import subprocess
import sys
import os
import time
import json

QEMU_CMD = "qemu-system-x86_64"
OVMF_CODE = "/usr/share/ovmf/OVMF.fd"
OVMF_VARS = "/usr/share/ovmf/OVMF_VARS.fd"
SERIAL_LOG = "bemi_serial.log"

def build_bios():
    print("[TEST] Building BEMI BIOS firmware...")
    result = subprocess.run(
        ["build", "-p", "BemiBiosPkg/BemiBiosPkg.dsc", "-a", "X64", "-t", "GCC5", "-b", "RELEASE"],
        capture_output=True, text=True, cwd=os.path.dirname(os.path.dirname(__file__))
    )
    if result.returncode != 0:
        print(f"[TEST] Build failed: {result.stderr}")
        return False
    print("[TEST] Build successful")
    return True

def run_qemu_test(boot_mode, timeout=30):
    ovmf_args = []
    if os.path.exists(OVMF_CODE):
        ovmf_args = ["-drive", f"if=pflash,format=raw,file={OVMF_CODE},readonly=on"]

    bemi_binary = "Build/BemiBios/RELEASE_GCC5/X64/BemiBiosCore.efi"

    cmd = [
        QEMU_CMD,
        "-machine", "q35,accel=kvm",
        "-cpu", "host",
        "-m", "4G",
        "-smp", "4",
        "-serial", f"file:{SERIAL_LOG}",
        "-nographic",
        "-no-reboot",
        "-netdev", "user,id=net0",
        "-device", "e1000,netdev=net0",
        "-drive", f"file={OVMF_VARS},if=pflash,format=raw",
    ] + ovmf_args

    if boot_mode == "lega
<truncated 2051 bytes>
1])
                if "BEMI threads" in line:
                    parts = line.split()
                    for i, p in enumerate(parts):
                        if p == "threads:" and i > 0:
                            results["bemi_threads"] = int(parts[i-1])
                if "ERROR" in line or "Error" in line:
                    results["errors"].append(line.strip())
    except FileNotFoundError:
        pass
    return results

def run_test_suite():
    print("=" * 60)
    print("BEMI BIOS — Production Test Suite")
    print("=" * 60)

    tests_passed = 0
    tests_total = 0

    test_cases = [
        ("native_boot", "native", 20),
        ("legacy_boot", "legacy", 30),
    ]

    for name, mode, timeout in test_cases:
        tests_total += 1
        print(f"\n--- Test: {name} ---")
        result = run_qemu_test(mode, timeout)
        if result:
            tests_passed += 1
            print(f"[PASS] {name}")
        else:
            print(f"[FAIL] {name}")

    print(f"\n{'=' * 60}")
    print(f"Results: {tests_passed}/{tests_total} passed")

    if os.path.exists(SERIAL_LOG):
        results = parse_serial_log()
        print(f"\nSerial log analysis:")
        print(f"  BEMI detected: {results.get('bemi_detected', False)}")
        print(f"  CPU count: {results.get('cpu_count', 0)}")
        print(f"  BEMI threads: {results.get('bemi_threads', 0)}")
        if results.get("errors"):
            print(f"  Errors: {len(results['errors'])}")
            for err in results["errors"][:5]:
                print(f"    {err}")

    return tests_passed == tests_total

if __name__ == "__main__":
    success = run_test_suite()
    sys.exit(0 if success else 1)
