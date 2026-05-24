#!/bin/bash
# BEMI BIOS USB Installer
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$SCRIPT_DIR/.."
DEPLOY_DIR="$PROJ_DIR/deploy"

USB_DEV="${1:-}"
BEMI_EFI="$DEPLOY_DIR/EFI/BOOT/BOOTX64.EFI"

if [ -z "$USB_DEV" ]; then
    echo "Usage: $0 /dev/sdX"
    echo "WARNING: This will format the target device!"
    lsblk -d -o NAME,SIZE,MODEL | grep -v loop
    exit 1
fi

echo "BEMI BIOS Installer"
echo "Target: $USB_DEV"
echo "WARNING: All data on $USB_DEV will be destroyed!"
read -p "Continue? (y/N) " confirm
if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
    echo "Aborted"
    exit 1
fi

echo "Formatting $USB_DEV as FAT32..."
sudo mkfs.vfat -F 32 -n BEMI_BIOS "$USB_DEV" 2>/dev/null || \
    sudo mkfs.fat -F 32 -n BEMI_BIOS "$USB_DEV"

echo "Creating EFI boot directory..."
MOUNT=$(mktemp -d)
sudo mount "$USB_DEV" "$MOUNT"
sudo mkdir -p "$MOUNT/EFI/BOOT"

echo "Installing BEMI BIOS..."
if [ -f "$BEMI_EFI" ]; then
    sudo cp "$BEMI_EFI" "$MOUNT/EFI/BOOT/BOOTX64.EFI"
    echo "BEMI BIOS installed as default UEFI bootloader"
else
    echo "ERROR: $BEMI_EFI not found. Build firmware first."
    sudo umount "$MOUNT"
    rmdir "$MOUNT"
    exit 1
fi

echo "Syncing..."
sync
sudo umount "$MOUNT"
rmdir "$MOUNT"

echo ""
echo "Installation complete!"
echo "Boot $USB_DEV on target system to run BEMI BIOS."
