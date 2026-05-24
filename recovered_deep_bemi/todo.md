# BEMI BIOS v1.3 — Production TODO

## Legend
- `[x]` = Done
- `[~]` = In progress
- `[ ]` = Not started

## Phase 0: Build System Remediation (COMPLETE)

- [x] 0.1 Add all 11 missing source files to `BemiBiosCore.inf`
- [x] 0.2 Create `SvmAsm.nasm` (VMRUN, VMSAVE, VMLOAD, STGI, CLGI, INVLPGA)
- [x] 0.3 Create `BemiSvmAsm.h` (extern declarations)
- [x] 0.4 Create `VmxExitAsm.nasm` (VM-exit trampoline — push GPRs, call C, pop, VMRESUME)
- [x] 0.5 Create `BemiVmxExitAsm.h` (extern declarations)
- [x] 0.6 Create `SvmExitAsm.nasm` (SVM #VMEXIT trampoline)
- [x] 0.7 Add include paths for all directories in `.inf`
- [x] 0.8 Set `PcdBemiEnable=TRUE`, `PcdBemiEnableHypervisorExperimental=TRUE` in `.dsc`
- [x] 0.9 Version bump: 1.2 → 1.3 across all files (`.dec`, `.dsc`, `.inf`, `BemiApi.h`, `BemiBiosCore.c`)

## Phase 1: Hypervisor Runtime (COMPLETE)

- [x] 1.1 Create `VmxExitAsm.nasm` — save/restore GPRs, call VmxExitDispatch, VMRESUME
- [x] 1.2 Create `SvmExitAsm.nasm` — SVM #VMEXIT trampoline
- [x] 1.3 Set host RSP (16KB stack per VCPU, top−128 red zone)
- [x] 1.4 Set host RIP = BemiVmxExitTrampoline
- [x] 1.5 Per-CPU host save area for SVM
- [x] 1.6 Implement `VmxRunVcpu()` (VMLAUNCH → loop handle exit → VMRESUME)
- [x] 1.7 Implement `SvmRunVcpu()` (VMRUN → loop handle #VMEXIT → resume)
- [x] 1.8 VM-exit dispatch in `VmxExitDispatch()` / `SvmExitDispatch()` (68-exit jump table)
- [x] 1.9 Build guest identity page tables (guest virtual 0-4G
<truncated 5613 bytes>
Complete INT 16h keyboard (UEFI SimpleTextInput)
- [ ] 6.4 INT 1Ah RTC/timer
- [ ] 6.5 INT 10h VESA extensions (4F00-4F03)
- [ ] 6.6 Real mode switch (64-bit → real mode)
- [ ] 6.7 MBR loading to 0x7C00 + jump to real mode entry
- [ ] 6.8 Bootloader scanning (enumerate devices, read MBR/GPT)
- [ ] 6.9 Boot menu

## Phase 7: ACPI + SMBIOS + Linux Boot

- [ ] 7.1 Create `hwcompat/acpi/AcpiTables.c` — RSDP, XSDT, MADT, FADT, DSDT, SSDT, MCFG, HPET
- [ ] 7.2 Create `hwcompat/smbios/SmbiosTables.c` — Type 0/1/4/7/16
- [ ] 7.3 Validate ACPI with acpica-tools (iasl -tc)
- [ ] 7.4 Hand off to Linux EFI stub (bootx64.efi or bzImage)
- [ ] 7.5 Intercept MOV CR3 → update EPT synchronously
- [ ] 7.6 Milestone: Linux kernel decompresses on serial
- [ ] 7.7 Milestone: Linux boots to initramfs, clean dmesg
- [ ] 7.8 UEFI ExitBootServices handler (lock state, release boot allocs)

## Phase 8: Testing

- [ ] 8.1 Instruction-level golden tests (decode→translate→execute→compare state)
- [ ] 8.2 Boot FreeDOS in QEMU
- [ ] 8.3 Boot Ubuntu 24.04 in QEMU
- [ ] 8.4 Boot Windows XP in QEMU (if CSM complete)
- [ ] 8.5 Coremark / STREAM / OpenSSL benchmarks
- [ ] 8.6 Fuzz testing (random x86 streams → compare with capstone)
- [ ] 8.7 72-hour stress test
- [ ] 8.8 Hardware validation (Intel 12th-14th gen, AMD Ryzen 7000+)

## Phase 9: Deployment

- [ ] 9.1 UEFI capsule update (signed, UpdateCapsule)
- [ ] 9.2 USB installer (bootable USB → copy to ESP → register boot entry)
- [ ] 9.3 Full BIOS image (`BEMIBIOS.fd`)
- [ ] 9.4 coreboot payload integration (cbfstool)
- [ ] 9.5 Fail-safe recovery (chain-load native if init fails)
- [ ] 9.6 UEFI HII setup screen (Enable/Disable, Boot Mode, Debug Level)
- [ ] 9.7 Hardware Compatibility List documentation
