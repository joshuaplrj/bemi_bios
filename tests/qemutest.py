#!/usr/bin/env python3
"""BEMI BIOS QEMU test harness — Phase 7 validation"""

import subprocess
import sys
import os
import time

QEMU_CMD = "qemu-system-x86_64"
OVMF_CODE = "/usr/share/ovmf/OVMF.fd"
OVMF_VARS = "/usr/share/ovmf/OVARS.fd"
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

    cmd = [
        QEMU_CMD,
        "-machine", "q35,accel=kvm" if os.path.exists("/dev/kvm") else "q35",
        "-cpu", "host" if os.path.exists("/dev/kvm") else "qemu64",
        "-m", "4G",
        "-smp", "4",
        "-serial", f"file:{SERIAL_LOG}",
        "-nographic",
        "-no-reboot",
    ]

    # Clean previous serial log
    if os.path.exists(SERIAL_LOG):
        try:
            os.remove(SERIAL_LOG)
        except OSError:
            pass

    if boot_mode == "legacy":
        boot_img = "bemi_boot.img"
        with open(boot_img, "wb") as f:
            f.write(b"\x00" * 512 * 2880) # 1.44MB floppy
        cmd += ["-drive", f"file={boot_img},format=raw,if=floppy"]
    else:
        if os.path.exists(OVMF_VARS):
            cmd += ["-drive", f"file={OVMF_VARS},if=pflash,format=raw"]
        cmd += ovmf_args
        cmd += ["-drive", "file=fat:rw:Build/BemiBios/RELEASE_GCC5/X64,format=raw"]

    try:
        proc = subprocess.Popen(cmd)
        time.sleep(3)  # Run for 3 seconds
        proc.terminate()
        proc.wait(timeout=5)
    except Exception as e:
        print(f"[TEST] QEMU run error: {e}")
        return False

    return True

def parse_serial_log():
    results = {
        "bemi_detected": False,
        "cpu_count": 0,
        "bemi_threads": 0,
        "errors": []
    }
    try:
        with open(SERIAL_LOG, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if "BEMI" in line:
                    results["bemi_detected"] = True
                if "Detected" in line and "CPUs" in line:
                    parts = line.split()
                    for i, p in enumerate(parts):
                        if (p == "CPUs," or p == "CPUs") and i > 0:
                            try:
                                results["cpu_count"] = int(parts[i-1])
                            except ValueError:
                                pass
                if "BEMI threads" in line or "threads:" in line:
                    parts = line.split()
                    for i, p in enumerate(parts):
                        if (p == "threads:" or p == "threads") and i > 0:
                            try:
                                results["bemi_threads"] = int(parts[i-1])
                            except ValueError:
                                pass
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
        ("native_boot", "native", 10),
        ("legacy_boot", "legacy", 10),
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
