# BEMI BIOS v1.3 — Production Deployment Plan

## Target: Real x86_64 (Intel + AMD) Systems | Full Legacy HW/SW Compatibility

---

## Decisions

| Decision | Choice |
|---|---|
| Architecture | **v1.3** — 4B ROB entries, 84 virtual threads, split/distributed ROB |
| Priority OS | **Linux first** (kernel 5.15 LTS), then Windows XP/7, then DOS |
| DBT Codegen | **Custom x86_64 assembler** — no external JIT dependency |
| Deployment | **Full BIOS replacement** (coreboot payload or standalone UEFI firmware image) |
| Timeline | **8-12 weeks** aggressive to first QEMU Linux boot |

---

## Critical Bug Repository (ALL FIXED)

| Bug | File | Severity | Status |
|---|---|---|---|
| Host RSP/RIP = 0 in VMCS | `VmxCore.c` | Fatal | **FIXED** — host stack + trampoline RIP |
| SVM has no assembly backend | Missing file | Fatal | **FIXED** — `SvmAsm.nasm` created |
| No VCPU run loop anywhere | `VmxCore.c`, `SvmCore.c` | Fatal | **FIXED** — `VmxRunVcpu`/`SvmRunVcpu` implemented |
| 11 source files not compiled | `BemiBiosCore.inf` | High | **FIXED** — all files added |
| PCD gates default FALSE | `.dec`/`.dsc` | High | **FIXED** — TRUE for dev builds |
| Trace cache hash overwrites | `TraceCache.c` | High | **FIXED** — linked list + spinlock |
| TAGE BTB no tag check | `TagePredictor.c` | High | **FIXED** — BTB tag verified vs RIP |
| Macro fusion Jcc never matches | `MacroOpFusion.c` | High | **FIXED** — 3-opcode parame
<truncated 734 bytes>
| Detail | Priority |
|---|---|---|---|
| 5.1 | Full primary opcode decoder | 0x00-0xFF all 250 opcodes, ModRM/SIB, all forms | High |
| 5.2 | Two/three-byte escape maps | 0x0F, 0x0F38, 0x0F3A, VEX, EVEX | High |
| 5.3 | Complete translator | All ALU, MOV, control flow, string, system, SIMD (passthrough) | High |
| 5.4 | Custom x86_64 codegen | Instruction encoders, register allocator, block chaining | High |
| 5.5 | Executable code cache | Alloc from EfiRuntimeServicesCode, I-cache flush | High |
| 5.6 | Connect codegen to trace cache | Store generated code pointers, hit→jump, miss→translate | Medium |

### Phase 6: Legacy CSM Complete

| # | Task | Detail | Priority |
|---|---|---|---|
| 6.1 | INT 13h disk I/O | Read/write/verify via EFI_BLOCK_IO, CHS→LBA, LBA48 | High |
| 6.2 | INT 15h memory | Real E820, A20 gate | High |
| 6.3 | INT 16h keyboard | UEFI SimpleTextInput scan code translation | High |
| 6.4 | Real mode switch | 64-bit → real mode transition | High |
| 6.5 | MBR chain-load | Load to 0x7C00, set IVT/BDA, jump to real mode | High |

### Phase 7: ACPI + SMBIOS + Linux Boot

| # | Task | Detail | Priority |
|---|---|---|---|
| 7.1 | ACPI table generation | RSDP, XSDT, MADT, FADT, DSDT, SSDT, MCFG, HPET | High |
| 7.2 | SMBIOS tables | Type 0/1/4/7/16 | Medium |
| 7.3 | Linux EFI stub handoff | Load bootx64.efi, intercept CR3, passthrough serial | High |
| 7.4 | ExitBootServices | Lock hypervisor state, release boot allocs | High |

### Phase 8-9: Testing + Deployment

| # | Task | Detail | Priority |
|---|---|---|---|
| 8.1 | 20+ test cases | All subsystems | Medium |
| 8.2 | Performance benchmarks | Coremark, STREAM, OpenSSL | Medium |
| 8.3 | USB installer / capsule update | Signed firmware, rollback | Medium |
| 8.4 | coreboot payload integration | cbfstool | Low |
