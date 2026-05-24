#!/usr/bin/env python3
"""Cross-platform USB installer for BEMI BIOS."""

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


class UsbDevice:
    def __init__(self, device, label, size, mounted=None):
        self.device = device
        self.label = label
        self.size = size
        self.mounted = mounted

    def __repr__(self):
        return f"{self.device} ({self.label}, {self.size // (1024**3)}GiB)"


def detect_drives():
    drives = []
    if platform.system() == "Linux":
        for dev in Path("/sys/block").iterdir():
            if dev.name.startswith("sd") and not dev.name.startswith("sda"):
                removable = (dev / "removable").read_text().strip()
                if removable == "1":
                    size = int((dev / "size").read_text().strip()) * 512
                    model = (dev / "device/model").read_text().strip() if (dev / "device/model").exists() else "USB"
                    drives.append(UsbDevice(f"/dev/{dev.name}", model, size))
    elif platform.system() == "Windows":
        import win32com.client
        wmi = win32com.client.Dispatch("WMI")
        for disk in wmi.InstancesOf("Win32_DiskDrive"):
            if disk.InterfaceType in ("USB", "USBx"):
                drives.append(UsbDevice(
                    disk.DeviceID,
<truncated 1370 bytes>
     ""]))
    print(f"[OK] startup.nsh written")


def verify_installation(esp_path):
    esp = Path(esp_path)
    efi_file = esp / "EFI" / "BOOT" / "BOOTX64.EFI"
    if not efi_file.exists():
        print("[FAIL] BOOTX64.EFI not found")
        return False
    sha256 = hashlib.sha256(efi_file.read_bytes()).hexdigest()
    print(f"[OK] Verification: SHA256={sha256}")
    return True


def main():
    parser = argparse.ArgumentParser(description="BEMI BIOS USB Installer")
    parser.add_argument("--device", help="Target USB device")
    parser.add_argument("--firmware", default="Build/RELEASE_GCC5/X64/BemiBiosCore.efi",
                        help="Path to BEMI firmware")
    parser.add_argument("--no-confirm", action="store_true")
    args = parser.parse_args()

    if not args.device:
        drives = detect_drives()
        if not drives:
            print("ERROR: No USB drives detected")
            sys.exit(1)
        print("Available drives:")
        for d in drives:
            print(f"  {d}")
        sys.exit("Specify --device")

    if not args.no_confirm:
        resp = input(f"WARNING: {args.device} will be wiped. Continue? [y/N] ")
        if resp.lower() != "y":
            sys.exit(0)

    if platform.system() == "Linux":
        create_gpt_layout(args.device)
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(["mount", f"{args.device}1", tmp])
            install_bemi_efi(tmp, args.firmware)
            write_startup_nsh(tmp)
            verify_installation(tmp)
            subprocess.run(["umount", tmp])
    else:
        print("Windows installer: mount ESP as USB drive, then re-run with --device <drive>:")


if __name__ == "__main__":
    main()
