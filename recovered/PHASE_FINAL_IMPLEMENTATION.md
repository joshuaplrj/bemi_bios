# BEMI BIOS v1.3 — Final Phase Implementation Plan
# QEMU Integration · Hardware Validation · USB Installer · Capsule Update · Coreboot Payload

---

## PART 1: EXISTING BUG AUDIT & IMPACT CATEGORIZATION

### 🔴 CRITICAL (Will crash/hang on real hardware or QEMU)

| # | File | Line | Bug | Fix |
|---|------|------|-----|-----|
| C1 | `VmxCore.c` | 29-31 | VMCS field encodings have extra digits: `0x00000681C`, `0x00000681E`, `0x000006800` — these are 5-nibble hex values, not valid 32-bit VMCS encodings. Should be `0x0000681C`, `0x0000681E`, `0x00006800`. | Correct all VMCS field `#define` values to proper 4-nibble Intel SDM encodings |
| C2 | `VmxCore.c` | 105 | `PACKED` attribute used as `} PACKED GDTR;` — EDK2 doesn't define `PACKED`. Should use `__attribute__((packed))` or rely solely on `#pragma pack(1)`. | Remove `PACKED` keyword; `#pragma pack(1)` already handles it |
| C3 | `VmxCore.c` | 394-401 | VMXON/VMCS/EPT/page-table regions use virtual addresses as physical addresses (`(UINT64)(UINTN)cpu->VmxonRegion`). These structs are in a pool allocation, not guaranteed page-aligned, and VA≠PA after paging. | Use `AllocateAlignedPages()` and convert via UEFI memory map or identity-mapped region |
| C4 | `SvmCore.c` | 128-129 | NPT control field at offset `0x500` stores `NptPml4 | 0x01` but SVM NPT enable is at VMCB offset `0x090` bit 0, not `0x500`. Offsets `0x500`/`0x510`/`0x520` are wrong per AMD APM Vol 2. | Correct all VMCB offsets to match AMD APM §15.5.1 |
| C5 
<truncated 20312 bytes>
 -n fallback/payload ...`

---

## PART 7: VERIFICATION MATRIX

| Test | Tool | Pass Criteria |
|------|------|---------------|
| VMLAUNCH round-trip (Intel) | QEMU + KVM | Serial: `BEMI_QEMU_TEST: PASS`, exit count ≥ 2 |
| VMRUN round-trip (AMD) | QEMU + KVM (AMD host) | Serial: `BEMI_QEMU_TEST: PASS` |
| FreeDOS boot | QEMU + freedos.img | Serial: `A:\>` prompt detected |
| Linux initramfs | QEMU + mini bzImage | Serial: `~ #` shell prompt |
| USB installer create | Physical USB + Linux host | `lsblk` shows ESP, `BOOTX64.EFI` present |
| USB boot | Physical machine | Serial: `BEMI BIOS v1.3 — Production Firmware` |
| Capsule update build | Build host | `.cap` file with valid PKCS#7 signature |
| Capsule apply | QEMU or physical | Version incremented after reboot |
| Capsule reject tampered | QEMU | Tampered capsule returns `EFI_SECURITY_VIOLATION` |
| Coreboot payload boot | QEMU + coreboot ROM | Serial: `BEMI: POST complete` |
| Intel 12th gen validation | Alder Lake hardware | All `ValVmx*` tests PASS |
| Intel 13th gen validation | Raptor Lake hardware | All `ValVmx*` tests PASS |
| AMD Zen4 validation | Ryzen 7000 hardware | All `ValSvm*` tests PASS |
| 72-hour stress | QEMU (continuous) | Zero crashes, zero memory leaks |

---

## PART 8: EXECUTION ORDER

1. **Fix critical bugs C1-C14** — Without these, nothing works
2. **Fix high bugs H1-H15** — Required for correct behavior
3. **QEMU round-trip test** — Proves VMLAUNCH/VMRESUME cycle
4. **Hardware validation framework** — Run on real Intel/AMD silicon
5. **USB installer** — Deploy to physical machines
6. **Capsule update** — Enable in-field firmware updates
7. **Coreboot payload** — Alternative deployment path

**Total new files: 48** | **Total modified files: ~15 (bug fixes)**
