# BEMI BIOS v7.2 — Production TODO

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
- [x] 1.9 Build guest identity page tables (guest virtual 0-4GB → guest physical 0-4GB)
- [x] 1.10 Build guest GDT (null, code 64-bit, data, 32-bit code, 32-bit data, TSS)
- [x] 1.11 Set guest segment registers (CS=0x08/0xA09B, DS/ES/SS=0x10/0xC093)
- [x] 1.12 Set guest CR3 to guest page tables
- [x] 1.13 Set guest GDTR/IDTR/TR/LDTR with limits and access rights
- [x] 1.14 Connect `VcpuCreate` and `VcpuRun` function pointers in `HypervisorBackend.c`
- [x] 1.15 Add VCPU test launch in `BemiBiosCore.c` (JMP $ infinite loop)
- [x] 1.16 FIX: Host RSP/RIP = 0 → set properly
- [x] 1.17 FIX: I/O bitmaps not zeroed → zeroed in VmxSetupVmcs

## Phase 2: v7.2 Architecture Alignment (COMPLETE)

- [x] 2.1 Thread count: 144 virtual threads via temporal SMT in v7.2
- [x] 2.2 Create `performance/rob/RobDistributor.c` (independent banks)
- [x] 2.3 Version bump to v7.2 across all files
- [x] 2.4 Update benchmark models to v7.2 (144 threads, 2B compressed ROB, pseudo-L4)

## Phase 3: Critical Bug Fixes (COMPLETE)

- [x] 3.1 FIX: TraceCache hash chain linking (overwrite → linked list + spinlock)
- [x] 3.2 FIX: TraceCache cross-bucket collision (walk by `next` pointer, not contiguous memory)
- [x] 3.3 FIX: TraceCache no spinlock → AcquireSpinLock/ReleaseSpinLock added
- [x] 3.4 FIX: TAGE BTB tag collision (check `gBtb[btbIdx].Tag == Rip >> 12` before returning target)
- [x] 3.5 FIX: Macro-op fusion Jcc detection (3-opcode parameter; `Opcode2==0x0F && Opcode3>=0x80`)
- [x] 3.6 FIX: MSR shadow read-only logic (EFI_ACCESS_DENIED on write to read-only MSRs)
- [x] 3.7 FIX: SMM state save/restore (real buffer, real TSC-based latency, no hardcoded 0s)
- [x] 3.8 FIX: SMM HypervisorStateBase/Size always set to 0 → proper save/restore
- [x] 3.9 FIX: SMM Validate always returned SUCCESS → real budget checking
- [x] 3.10 FIX: Remove unused `CPUID_RETURN` typedef from PostRoutines.c

## Phase 4: Hardware Compatibility

### CPUID
- [ ] 4.1 Complete leaf 0x00-0x1F (all standard leaves)
- [ ] 4.2 Complete leaf 0x40000000-0x4FFFFFFF (BEMI hypervisor interface)
- [ ] 4.3 Complete leaf 0x80000000-0x8000001F (AMD extended)
- [ ] 4.4 Create CpuidProfile with per-OS presets

### MSR
- [ ] 4.5 Shadow MTRRs (0x200-0x2FF)
- [ ] 4.6 Shadow PAT (0x277)
- [ ] 4.7 Shadow debug MSRs (0x1D9)
- [ ] 4.8 Shadow perf counters (0x186, 0xC1)

### APIC
- [ ] 4.9 Connect APIC shadow writes to interrupt injection via VMCS entry-interruption info
- [ ] 4.10 Implement LAPIC timer (TSC deadline, one-shot, periodic)
- [ ] 4.11 Complete PIC 8259A cascade (master+slave, ICW/OCW, IRR/ISR/IMR)
- [ ] 4.12 I/O APIC emulation (MMIO at 0xFEC00000, IOREDTBL)

## Phase 5: DBT Pipeline

### Decoder
- [ ] 5.1 Primary opcode map 0x00-0xFF (all ~250 opcodes)
- [ ] 5.2 Two-byte escape 0x0F (MOVZX/MOVSX, SETcc, CMOVcc, BSWAP, BT/SHLD/SHRD)
- [ ] 5.3 Three-byte maps 0x0F38/0x0F3A (SSE4, AES, SHA, CRC32, BMI1/2)
- [ ] 5.4 VEX encoding (AVX/AVX2/FMA, 2-byte + 3-byte, YMM)
- [ ] 5.5 EVEX encoding (AVX-512: F/CD/ER/PF/BW/DQ/VL/IFMA/VBMI)
- [ ] 5.6 x87 FPU (0xD8-0xDF: FADD/FSUB/FMUL/FDIV, FLD/FST, FSIN/FCOS)
- [ ] 5.7 Full ModRM/SIB/VSIB decoding
- [ ] 5.8 Fuzz test against capstone/xed

### IR Upgrades
- [ ] 5.9 Add SIMD register file (XMM0-31, YMM0-31, ZMM0-31, K0-7, MXCSR)
- [ ] 5.10 Add operand size field (8/16/32/64/128/256/512)
- [ ] 5.11 Add memory addressing (LoadMR/StoreMR with base+index*scale+disp)
- [ ] 5.12 Decompose EFLAGS (individual CF/PF/AF/ZF/SF/OF with lazy eval)
- [ ] 5.13 Expand temp reg pool (RTmp0-2 → RTmp0-15)
- [ ] 5.14 Increase translated block capacity (256 → 1024 micro-ops)

### Translation
- [ ] 5.15 Fix direction bit in ALU translation
- [ ] 5.16 Fix ADC/SBB → distinct carry micro-ops
- [ ] 5.17 Complete ALU translation (all forms)
- [ ] 5.18 Complete MOV translation (all forms)
- [ ] 5.19 Complete control flow (JMP, Jcc, CALL, RET, LOOP, INT, IRET)
- [ ] 5.20 Complete shift/rotate/stack ops
- [ ] 5.21 Complete string ops (microcoded loops, REP, DF)
- [ ] 5.22 SIMD passthrough (MacroOpPassthrough with register remap)
- [ ] 5.23 System instructions (CPUID, RDMSR, WRMSR, HLT, CLI, STI, INVLPG)
- [ ] 5.24 Self-modifying code (write-protect → invalidate translations)

### Code Generation
- [ ] 5.25 Create `dbt/codegen/mod.rs` — instruction encoders (REX, VEX, EVEX, ModRM, SIB)
- [ ] 5.26 Create `dbt/codegen/regalloc.rs` — linear scan register allocator
- [ ] 5.27 Create `dbt/codegen/block.rs` — prologue/epilogue, block chaining
- [ ] 5.28 Create `dbt/codegen/cache.rs` — executable code cache (alloc, emit, flush)
- [ ] 5.29 ALU encoders (mov, add, sub, and, or, xor, cmp, test)
- [ ] 5.30 Memory encoders (mov_mr, mov_rm, lea)
- [ ] 5.31 Branch encoders (jmp_rel, jcc_rel, call_rel, ret)
- [ ] 5.32 Stack encoders (push, pop)
- [ ] 5.33 SIMD passthrough (copy native bytes, optional reg remap)
- [ ] 5.34 Indirect branch cache (inline CMP+JE, fallback to lookup)
- [ ] 5.35 Exception table (code offset → guest RIP mapping)
- [ ] 5.36 I-cache coherency (CLFLUSH + MFENCE after code emit)
- [ ] 5.37 Replace interpreter dispatch with codegen calls

## Phase 6: Legacy CSM + Boot

- [ ] 6.1 Complete INT 13h read/write/verify via EFI_BLOCK_IO
- [ ] 6.2 Complete INT 15h E820 memory map + A20 gate
- [ ] 6.3 Complete INT 16h keyboard (UEFI SimpleTextInput)
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
