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
| C5 | `SvmCore.c` | 137 | `BemiSvmExitTrampoline` is referenced but never declared in the header (`BemiVmxExitAsm.h` only declares VMX trampoline). | Add `extern VOID BemiSvmExitTrampoline(VOID);` declaration |
| C6 | `SvmAsm.nasm` | 8,18,28 | `vmrun [rcx]`, `vmsave [rcx]`, `vmload [rcx]` — these instructions take the VMCB PA in RAX, not memory operand `[rcx]`. Correct form: `mov rax, [rcx]` then `vmrun`. | Fix to `mov rax, [rcx]; vmrun` pattern |
| C7 | `SvmAsm.nasm` | 50 | `invlpga [rcx], edx` — INVLPGA takes RAX (address) and ECX (ASID), not `[rcx], edx`. | Fix to `mov rax, rcx; mov ecx, edx; invlpga` |
| C8 | `VmxExitAsm.nasm` | 50-56 | After VMRESUME, if it succeeds execution never returns (guest resumes). The `xor eax,eax; ret` after VMRESUME is dead code. If VMRESUME fails, the `jbe .resume_fail` is correct, but there's no error diagnostics. | Add VMREAD of VM_INSTRUCTION_ERROR after failure |
| C9 | `HypervisorBackend.c` | 129 | MSR write exit reason hardcoded as `0x1C` but Intel SDM defines WRMSR exit as reason 32 (`0x20`), RDMSR as 31 (`0x1F`). `0x1C`=CR access. | Fix exit reason constants to match Intel SDM Vol 3 Appendix C |
| C10 | `HypervisorBackend.c` | 173 | CR access qualification parsing: `crNumber = (qual >> 4) & 0x0F` — Intel SDM says CR number is bits 3:0, access type is bits 5:4. Reversed. | Fix to `crNumber = qual & 0x0F; accessType = (qual >> 4) & 0x03;` |
| C11 | `HypervisorBackend.c` | 204 | I/O exit: direction bit is `(qual & 0x04)` but SDM says bit 3 is direction (0=OUT, 1=IN). Size is bits 2:0. | Fix bit extraction per Intel SDM §25.1.2 |
| C12 | `BemiBiosPkg.dec` | 16-20 | PCD declarations use `PcdsDynamic, PcdsDynamicEx` section but provide default values inline (`BOOLEAN| TRUE`). EDK2 requires `PcdsFixedAtBuild` or proper `|default` syntax for `.dec`. | Move to `[PcdsFixedAtBuild]` or use correct syntax |
| C13 | `AcpiTables.c` | 478 | `dsdt->Header.Checksum` — but `dsdt` is `ACPI_SDT_HEADER*`, which has no `.Header` member. It IS the header. Should be `dsdt->Checksum`. | Fix to `dsdt->Checksum = AcpiCalcChecksum(...)` |
| C14 | `AcpiTables.c` | 501-506 | RSDP is copied to hardcoded GPA `0x000E0000`. On real hardware this is ROM space; on QEMU you must install via `EFI_ACPI_TABLE_PROTOCOL` or use UEFI config tables. Direct write will fault. | Use `gBS->InstallConfigurationTable(&gEfiAcpiTableGuid, ...)` |

### 🟡 HIGH (Incorrect behavior, data corruption, or silent failures)

| # | File | Bug |
|---|------|-----|
| H1 | `VmxCore.c:227` | VMCS revision ID written as full 32-bit `vmxBasicMsr & 0xFFFFFFFF` but SDM says bits 30:0 only; bit 31 must be 0. |
| H2 | `VmxCore.c:170-176` | Guest page tables: PDE entry `0x09` = Present+PS(2MB page) but missing R/W bit. Should be `0x83` (P+RW+PS). Guest writes will #PF. |
| H3 | `VmxCore.c:250-265` | Pin/Proc/Exit/Entry control magic numbers (`0x6F`, `0x0D174FE6`, etc.) are opaque and likely wrong for most CPUs. Must be derived from capability MSRs only. |
| H4 | `VmxCore.c:287-290` | MSR bitmap: only first 4 bytes set to `0xFF`, intercepts MSRs 0x00-0x1F only. Most critical MSRs (EFER, APIC_BASE, STAR, LSTAR) are above this range and will pass through. |
| H5 | `SvmCore.c:113-122` | VMCB intercept setup uses byte offsets into a `0x018` base but the intercept fields are 32-bit words at VMCB offsets 0x000-0x014. Byte-level OR is wrong. |
| H6 | `SmmHandler.c:107` | `WriteIo8` is not an EDK2 function. Should be `IoWrite8`. |
| H7 | `SmmHandler.c:143` | Same: `WriteIo8` → `IoWrite8`. |
| H8 | `SmbiosTables.c:166-192` | Type 0: header is written via `gSmbiosOffset += sizeof(t0)` but never actually copied to `gSmbiosMemory`. The struct data is lost. |
| H9 | `SmbiosTables.c:214,259` | Same pattern for Type 1 and Type 4 — struct data never written to buffer. |
| H10 | `CsmModule.c:175` | `volatile UINT16 *videoMode` assigned from `volatile UINT8 *` cast — type mismatch. |
| H11 | `CpuidSpoof.c:157` | Hypervisor leaf `0x40000001` EAX uses `BEMI_MAJOR_VERSION` but this macro is in `BemiApi.h` which is not included in `CpuidSpoof.c`. |
| H12 | `ApicShadow.c:176-180` | Timer divider calculation: `shift = dcr & 0x0B` is wrong mask. DCR divider encoding uses bits 3,1,0 (not contiguous). |
| H13 | `build.sh:9` | Version string says "v1.2" but project is v1.3. |
| H14 | `Cargo.toml:3` | `version = "1.2.0"` — should be `1.3.0`. |
| H15 | `PostAsm.nasm` | Contains VMX instruction wrappers (VMXON, VMCLEAR, etc.) that duplicate what should be in a separate VMX-only ASM file. Dual definitions will cause linker errors. |

### 🟢 LOW (Style, maintainability, non-blocking)

| # | File | Bug |
|---|------|-----|
| L1 | `BemiBiosCore.c:142-146` | `gBS->FlushInstructionCache` with `gBS->ImageHandle` as first arg — this is a CPU arch protocol call, not a BS call. |
| L2 | `HypervisorBackend.c:178-190` | CR access handler always reads/writes RAX regardless of the actual register operand specified in exit qualification bits 11:8. |
| L3 | `dbt/codegen/mod.rs:92` | `emit_mov_rr` has REX.R and REX.B swapped (r=is_r8(dst), b=is_r8(src)). For MOV r/m, r: REX.R extends reg, REX.B extends r/m. dst=r/m needs B, src=reg needs R. |
| L4 | `dbt/decoder/mod.rs:218` | `imm_w` always calls `imm32` regardless of `wide` flag — 16-bit operand sizes never handled. |
| L5 | `TestSuite.c` | Tests only verify host CPU features, not BEMI spoofed values. Not useful as regression tests. |
| L6 | `QemuTest.py:45` | OVMF_VARS used as pflash but also written; should be copied to temp first. |
| L7 | `ApicShadow.c:352-363` | ISR/IRR range checks overlap: both `offset >= 0x100` and `offset >= 0x200` blocks match for IRR offsets, ISR block catches IRR too. |
| L8 | `dbt/lib.rs` | `pub use codegen::*` missing — codegen module declared but not re-exported. |


## PART 2: QEMU INTEGRATION TEST (Verify VMLAUNCH Round-Trip)

### File Structure

```
pro-tes/tests/
├── qemu/
│   ├── QemuRoundTrip.c          [NEW]  UEFI app: VMLAUNCH→exit→VMRESUME→HLT→report
│   ├── QemuRoundTrip.inf        [NEW]  INF for standalone UEFI shell test app
│   ├── QemuTestHarness.py       [NEW]  Python: launch QEMU, parse serial, assert PASS/FAIL
│   ├── QemuGuestPayloads.nasm   [NEW]  Guest code blobs (HLT, CPUID, IO-out)
│   ├── QemuExpectedResults.json [NEW]  Golden expected serial output per test case
│   └── conftest.py              [NEW]  pytest fixtures for QEMU process management
├── vmx/
│   ├── VmxLaunchTest.c          [NEW]  VMXON→VMCLEAR→VMPTRLD→VMLAUNCH unit test
│   ├── VmxExitTest.c            [NEW]  Force each exit reason, verify dispatch
│   └── VmxFieldTest.c           [NEW]  VMREAD/VMWRITE roundtrip for all fields
├── svm/
│   ├── SvmRunTest.c             [NEW]  VMRUN round-trip on AMD
│   └── SvmExitTest.c            [NEW]  Force SVM exits, verify dispatch
└── integration/
    ├── BootFreeDOS.py            [NEW]  Boot FreeDOS floppy image, check serial
    ├── BootLinuxStub.py          [NEW]  Boot minimal Linux bzImage to initramfs
    └── StressTest.py             [NEW]  72-hour soak: random guest workloads
```

### Key File Details

**`QemuRoundTrip.c`** — UEFI_APPLICATION
- `RoundTripTestEntry()` — Detect vendor, dispatch to VMX or SVM test path
- `SetupTestVmcs()` — Minimal VMCS with 5-instruction guest (MOV, CPUID, HLT)
- `ValidateExitChain()` — Expects CPUID exit → spoof → VMRESUME → HLT exit
- `ReportResult()` — Writes `BEMI_QEMU_TEST: PASS` or `FAIL:<reason>` to serial
- `CleanupVmx()` — VMXOFF, free all allocated pages

**`QemuTestHarness.py`** — Python test orchestrator
- `class QemuInstance` — Context manager: start QEMU with OVMF + test EFI
- `launch(firmware, guest_image, timeout, kvm)` — subprocess with serial pipe
- `wait_for_marker(marker, timeout)` — Scan serial for PASS/FAIL marker
- `assert_no_triple_fault()` — Detect QEMU reset or triple fault in log
- `run_test_matrix()` — Run all payloads, output JUnit XML

**`QemuGuestPayloads.nasm`** — Position-independent guest blobs (≤64B each)
- `payload_hlt` — `mov eax, 0xBEMI; hlt`
- `payload_cpuid` — `mov eax, 1; cpuid; hlt`
- `payload_io_serial` — `mov dx, 0x3F8; mov al, 'B'; out dx, al; hlt`
- `payload_msr_read` — `mov ecx, 0xC0000080; rdmsr; hlt`
- `payload_cr3_write` — `mov rax, cr3; mov cr3, rax; hlt`

**`VmxLaunchTest.c`** — VMX unit tests
- `TestVmxOnOff()` — VMXON with valid revision → VMXOFF; assert success
- `TestVmcsClear()` — VMCLEAR + VMPTRLD; assert no VMfailValid
- `TestVmLaunchMinimal()` — HLT-only guest, assert exit reason == 0x0C
- `TestVmResumeAfterExit()` — Handle HLT, advance RIP, VMRESUME, verify 2nd exit
- `TestVmcsFieldRoundTrip()` — VMWRITE → VMREAD → compare for 40+ fields

**`VmxExitTest.c`** — Per-exit-reason verification
- `TestExitCpuid()` — Verify exit reason 10 (0x0A), RAX spoofed
- `TestExitRdmsr()` — Verify exit reason 31 (0x1F), not 0x1C
- `TestExitWrmsr()` — Verify exit reason 32 (0x20)
- `TestExitIo()` — Verify exit reason 30, port in qualification
- `TestExitCr()` — Verify exit reason 28, CR number in bits 3:0
- `TestExitEptViolation()` — Verify exit reason 48 (0x30)

---

## PART 3: HARDWARE VALIDATION (Intel 12-14th Gen, AMD Ryzen 7000+)

### File Structure

```
pro-tes/hwval/                                [NEW directory]
├── HwValRunner.c                             [NEW]  Main validation entry
├── HwValRunner.inf                           [NEW]  UEFI shell app INF
├── intel/
│   ├── AlderLakeVal.c                        [NEW]  12th gen (P/E-core hybrid)
│   ├── RaptorLakeVal.c                       [NEW]  13th gen
│   ├── MeteorLakeVal.c                       [NEW]  14th gen
│   └── IntelCommonVal.c                      [NEW]  VMX cap MSR checks
├── amd/
│   ├── Zen4Val.c                             [NEW]  Ryzen 7000
│   ├── Zen5Val.c                             [NEW]  Ryzen 9000
│   └── AmdCommonVal.c                        [NEW]  SVM/NPT cap checks
├── common/
│   ├── CpuFeatureProbe.c                     [NEW]  CPUID enumeration
│   ├── MsrProbe.c                            [NEW]  Safe MSR read with #GP
│   ├── TopologyProbe.c                       [NEW]  P-core/E-core/SMT detect
│   └── TimingProbe.c                         [NEW]  TSC freq, VMLAUNCH latency
└── report/
    ├── HwValReport.c                         [NEW]  JSON/text report generator
    └── HwCompatList.md                       [NEW]  Auto-generated HCL
```

### Key File Details

**`HwValRunner.c`** — Validation orchestrator
- `HwValEntryPoint()` — Detect vendor, dispatch Intel/AMD suite
- `HwValRunAllTests()` — Execute all registered tests, collect pass/fail
- `HwValSerialReport()` — Structured JSON to serial 0x3F8
- Struct: `HW_VAL_RESULT { Name[64]; Passed; Latency; Detail[256]; }`

**`IntelCommonVal.c`** — VMX capability validation
- `ValVmxCapability()` — IA32_VMX_BASIC: revision, VMCS size, memory type
- `ValVmxControls()` — TRUE_PINBASED/PROCBASED/EXIT/ENTRY capability MSRs
- `ValEptCapability()` — IA32_VMX_EPT_VPID_CAP: 2MB/1GB page support
- `ValVmxLaunchLatency()` — 1000× VMLAUNCH→HLT-exit cycles; avg/p99

**`AlderLakeVal.c`** — Hybrid topology tests
- `ValHybridTopology()` — CPUID 0x1A: P-core vs E-core; BEMI thread mapping
- `ValThreadDirector()` — HFI/ITD MSRs not corrupted by hypervisor

**`AmdCommonVal.c`** — SVM capability validation
- `ValSvmCapability()` — CPUID 0x8000000A: SVM rev, ASID count, NPT, NRIP
- `ValNptCapability()` — NPT support, large pages
- `ValSvmRunLatency()` — 1000× VMRUN→HLT-exit cycles; avg/p99

---

## PART 4: USB INSTALLER

### File Structure

```
pro-tes/deploy/
├── usb/
│   ├── BemiUsbInstaller.sh          [NEW]  Main installer script (Linux)
│   ├── BemiUsbInstaller.ps1         [NEW]  Windows PowerShell installer
│   ├── BemiUsbInstaller.py          [NEW]  Cross-platform Python installer
│   ├── partition_layout.cfg         [NEW]  GPT partition table definition
│   ├── startup.nsh                  [NEW]  UEFI shell auto-run script
│   ├── grub/
│   │   ├── grub.cfg                 [NEW]  GRUB2 config for BEMI chain-load
│   │   └── install_grub.sh          [NEW]  Install GRUB to ESP
│   └── recovery/
│       ├── RecoveryShell.efi        [NEW]  Fallback UEFI shell binary
│       └── recovery_config.json     [NEW]  Recovery mode parameters
```

### Key File Details

**`BemiUsbInstaller.py`** — Cross-platform installer
- `class UsbDevice` — Detect removable USB drives, get size/model
- `detect_drives()` — List USB block devices (Linux: /sys/block, Win: WMI)
- `create_gpt_layout(device)` — Create GPT with ESP (512MB FAT32) + data partition
- `install_bemi_efi(esp_path)` — Copy BemiBiosCore.efi to ESP/EFI/BOOT/BOOTX64.EFI
- `install_recovery(esp_path)` — Copy recovery shell + fallback native chain-loader
- `write_startup_nsh(esp_path)` — Create startup.nsh for auto-load
- `register_boot_entry(disk)` — Use efibootmgr (Linux) or bcdedit (Win) to add boot entry
- `verify_installation(esp_path)` — SHA256 verify all installed files
- `main()` — argparse CLI: `--device`, `--firmware`, `--no-confirm`, `--recovery-only`

**`startup.nsh`** — UEFI Shell auto-run
- Load BemiBiosCore.efi from ESP
- If load fails, print error and drop to UEFI shell
- Set 5-second timeout before auto-boot

**`partition_layout.cfg`** — GPT definition
- Partition 1: EFI System Partition, 512MB, FAT32, type `C12A7328-...`
- Partition 2: BEMI Data, remaining space, ext4/FAT32, stores logs + trace cache dumps

---

## PART 5: CAPSULE UPDATE

### File Structure

```
pro-tes/deploy/
├── capsule/
│   ├── CapsuleHeader.c              [NEW]  Build EFI_CAPSULE_HEADER + FMP payload
│   ├── CapsuleHeader.h              [NEW]  Capsule GUID, version, flag definitions
│   ├── CapsuleSigning.py            [NEW]  Sign capsule with PKCS#7 / OpenSSL
│   ├── CapsuleBuilder.py            [NEW]  Assemble: header + signed payload + FMP
│   ├── CapsuleApply.c               [NEW]  DXE driver: handle UpdateCapsule() RT call
│   ├── CapsuleApply.inf             [NEW]  INF for capsule handler DXE driver
│   ├── CapsuleVerify.c              [NEW]  Verify signature before flash
│   ├── keys/
│   │   ├── generate_keys.sh         [NEW]  Generate RSA-2048 key pair
│   │   ├── test_signing_key.pem     [NEW]  Dev-only test key (DO NOT ship)
│   │   └── README_KEYS.md           [NEW]  Key management instructions
│   └── test/
│       ├── CapsuleRoundTrip.py      [NEW]  Build→sign→apply→verify test
│       └── CapsuleCorruptTest.py    [NEW]  Tampered capsule must be rejected
```

### Key File Details

**`CapsuleBuilder.py`** — Capsule assembly pipeline
- `class FmpPayload` — Firmware Management Protocol payload structure
- `build_capsule(efi_binary, version, key_path)` — Assemble full capsule image
- `compute_payload_hash(binary)` — SHA-256 of firmware binary
- `attach_fmp_header(payload, version, lowest_supported)` — FMP descriptor
- `sign_payload(payload, key_pem)` — PKCS#7 detached signature via OpenSSL
- `write_capsule(output_path, header, signed_payload)` — Final .cap file
- **Output**: `BEMI_UPDATE_v1.3.x.cap`

**`CapsuleApply.c`** — Runtime capsule handler
- `CapsuleApplyEntryPoint()` — Register as `gEfiFirmwareManagementProtocolGuid` handler
- `BemiCapsuleCheckImage()` — Verify GUID, version ≥ current, signature valid
- `BemiCapsuleSetImage()` — Write verified payload to SPI flash region
- `BemiCapsuleGetInfo()` — Report current firmware version, last attempt status
- `ValidateSignature(payload, sig, sigLen)` — RSA-2048 + SHA-256 verification
- `FlashWriteRegion(base, size, data)` — SPI flash write with block erase

**`CapsuleVerify.c`** — Pre-flash verification
- `VerifyCapsuleIntegrity(capsule)` — Check header magic, size, CRC32
- `VerifyCapsuleSignature(capsule, trustedKey)` — PKCS#7 chain verification
- `VerifyVersionPolicy(capsule, currentVer)` — Reject downgrades unless forced
- `VerifyTargetPlatform(capsule)` — Check platform GUID matches this board

---

## PART 6: COREBOOT PAYLOAD INTEGRATION

### File Structure

```
pro-tes/deploy/
├── coreboot/
│   ├── BemiPayload.c                [NEW]  Coreboot payload entry (replaces Tianocore)
│   ├── BemiPayload.h                [NEW]  Coreboot table parsing definitions
│   ├── CbMemTable.c                 [NEW]  Parse coreboot memory table → E820
│   ├── CbSerialInit.c               [NEW]  Initialize serial from coreboot UART info
│   ├── CbFdt.c                      [NEW]  Parse FDT if present (ARM compat path)
│   ├── Makefile.coreboot            [NEW]  Build BEMI as coreboot ELF payload
│   ├── bemi.kconfig                 [NEW]  Kconfig fragment for coreboot menuconfig
│   ├── cbfstool_add.sh              [NEW]  Script: cbfstool add BEMI to ROM image
│   └── test/
│       ├── CbPayloadQemuTest.py     [NEW]  QEMU + coreboot ROM + BEMI payload
│       └── CbTableParseTest.c       [NEW]  Unit test coreboot table parser
```

### Key File Details

**`BemiPayload.c`** — Coreboot payload entry point
- `BemiPayloadMain(cb_header*)` — Entry from coreboot; receives coreboot table pointer
- `ParseCorebootTables(header)` — Walk coreboot table chain (LB_TAG_MEMORY, LB_TAG_SERIAL, LB_TAG_FRAMEBUFFER)
- `InitFromCorebootMemmap(cb_mem)` — Build internal memory map from coreboot ranges
- `TransitionToUefiRuntime()` — Set up minimal UEFI boot/runtime services for BEMI DXE
- `LaunchBemiCore()` — Call BemiBiosEntryPoint() with synthesized SystemTable

**`CbMemTable.c`** — Memory table conversion
- `CbParseMemoryTable(table)` — Convert `lb_memory` entries to E820 format
- `CbFindHighMemory(map, minSize)` — Find largest usable region for BEMI heap
- `CbReserveRegion(map, base, size)` — Mark BEMI regions as reserved

**`Makefile.coreboot`** — Build integration
- `bemi_payload.elf` target: compile all BEMI C sources + link as ELF32
- Linker script: entry = `BemiPayloadMain`, load at 0x10000000
- `cbfstool_add` target: add ELF to coreboot ROM as `img/bemi`
- `coreboot_config` target: apply bemi.kconfig to coreboot `.config`

**`cbfstool_add.sh`** — ROM integration script
- `cbfstool $ROM add-payload -f bemi_payload.elf -n img/bemi -c lzma`
- Verify with `cbfstool $ROM print`
- Optional: set as primary payload via `cbfstool $ROM add -n fallback/payload ...`

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
