#!/usr/bin/env python3
"""Integration test: Boot FreeDOS floppy via BEMI."""

import sys
import subprocess
import tempfile
from pathlib import Path


def test_boot_freedos(firmware_path, floppy_path):
    with tempfile.TemporaryDirectory() as tmp:
        result = subprocess.run([
            "qemu-system-x86_64", "-nographic",
            "-machine", "accel=kvm",
            "-cpu", "host",
            "-m", "512M",
            "-drive", f"if=pflash,format=raw,file={firmware_path}",
            "-fda", floppy_path,
            "-serial", "stdio",
            "-monitor", "none"
        ], capture_output=True, timeout=30)

        output = result.stdout.decode(errors="replace")
        if "A:" in output or "BEMI" in output:
            print("[PASS] FreeDOS boot detected")
            return True
        print("[FAIL] No boot evidence in serial output")
        return False


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: BootFreeDOS.py <firmware.efi> <freedos.img>")
        sys.exit(1)
    sys.exit(0 if test_boot_freedos(sys.argv[1], sys.argv[2]) else 1)
