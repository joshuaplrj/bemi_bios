#!/usr/bin/env python3
"""Cross-platform USB installer for BEMI BIOS."""

import argparse
import hashlib
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
        try:
            import win32com.client
            wmi = win32com.client.Dispatch("WMI")
            for disk in wmi.InstancesOf("Win32_DiskDrive"):
                if disk.InterfaceType in ("USB", "USBx"):
                    size = int(disk.Size) if disk.Size else 0
                    drives.append(UsbDevice(disk.Index, disk.Model or "USB", size))
        except Exception as e:
            print(f"[WARN] Failed to use WMI for drive detection: {e}")
    return drives

def create_gpt_layout(device):
    print(f"[DISK] Creating GPT layout on {device}...")
    if platform.system() == "Linux":
        subprocess.run(["parted", "-s", device, "mklabel", "gpt"])
        subprocess.run(["parted", "-s", device, "mkpart", "ESP", "fat32", "1MiB", "513MiB"])
        subprocess.run(["parted", "-s", device, "set", "1", "esp", "on"])
        subprocess.run(["mkfs.vfat", "-F", "32", "-n", "ESP", f"{device}1"])
    elif platform.system() == "Windows":
        script = f"""
        select disk {device}
        clean
        convert gpt
        create partition efi size=512
        format quick fs=fat32 label=ESP
        assign letter=S
        """
        script_path = "diskpart.txt"
        with open(script_path, "w") as f:
            f.write(script)
        subprocess.run(["diskpart", "/s", script_path])
        try:
            os.remove(script_path)
        except OSError:
            pass

def install_bemi_efi(esp_path, firmware_path):
    print(f"[COPY] Copying BEMI EFI binary to {esp_path}...")
    dest = Path(esp_path) / "EFI" / "BOOT"
    dest.mkdir(parents=True, exist_ok=True)
    if os.path.exists(firmware_path):
        shutil.copy2(firmware_path, dest / "BOOTX64.EFI")
        print(f"[OK] BOOTX64.EFI copied")
    else:
        # Create a dummy BOOTX64.EFI for testing if it doesn't exist yet
        with open(dest / "BOOTX64.EFI", "wb") as f:
            f.write(b"BEMIBIOS")
        print(f"[WARN] Firmware binary not found. Wrote stub BOOTX64.EFI for testing")

def write_startup_nsh(esp_path):
    esp = Path(esp_path)
    startup = esp / "startup.nsh"
    with open(startup, "w") as f:
        f.write("@echo -off\n")
        f.write("fs0:\\EFI\\BOOT\\BOOTX64.EFI\n")
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
    parser.add_argument("--device", help="Target USB device (e.g. /dev/sdb on Linux, or Disk Index on Windows)")
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
        create_gpt_layout(args.device)
        # S: is assigned in diskpart script
        install_bemi_efi("S:", args.firmware)
        write_startup_nsh("S:")
        verify_installation("S:")
        # Unassign S: drive letter
        script = f"select disk {args.device}\nselect partition 1\nremove letter=S\n"
        with open("diskpart_cleanup.txt", "w") as f:
            f.write(script)
        subprocess.run(["diskpart", "/s", "diskpart_cleanup.txt"])
        try:
            os.remove("diskpart_cleanup.txt")
        except OSError:
            pass
        print("[OK] USB installation complete")

if __name__ == "__main__":
    main()
