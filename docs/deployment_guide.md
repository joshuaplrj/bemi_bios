# BEMI BIOS -- Deployment Guide

## Prerequisites

- EDK2 build environment
- NASM 2.15+
- LLVM 18+ / GCC 12+
- Rust nightly with `x86_64-unknown-uefi` target
- QEMU 7.0+ (for testing)

## Build

```bash
cd pro-tes
make -C deploy firmware
```

## QEMU Testing

```bash
# Native boot (Mode A)
make -C deploy qemu

# Legacy boot (Mode B)
make -C deploy qemu-legacy
```

## Hardware Deployment

1. Build the firmware: `make -C deploy`
2. Copy `deploy/EFI/BOOT/BOOTX64.EFI` to a FAT32 USB drive
3. Boot the target system and select the USB drive from UEFI boot menu
4. BEMI BIOS will detect boot mode and initialize accordingly

## Boot Modes

- **Mode A (Native)**: Skips Ring -1 initialization, chain-loads native OS
- **Mode B (Legacy)**: Initializes Ring -1 hypervisor, intercepts x86 execution

## v1.3 ROB Entry Density Support

Bemi v1.3 (ROB Entry Density) is supported on any Bemi v1.2-compatible deployment. The ROB
density advantage (4B entries vs x86 14B = 3.5x more threads per SRAM byte) is a firmware-
level parameter: the number of virtual threads advertised to the backend is reduced from 144
(v1.2) to 84 (v1.3). No hardware changes are required -- the split/distributed ROB model is
a configuration of the existing RISC execution unit clustering.

Configure via `BEMI_NATIVE` EFI variable.

