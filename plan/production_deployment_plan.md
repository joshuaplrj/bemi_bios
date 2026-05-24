# BEMI BIOS — Production Deployment Plan
## Deploying to Real x86 (AMD + Intel) Systems with Full Legacy Compatibility

> **Scope**: Transform the current prototype (`bios_prototype.py`, `hybrid_bemi/`, benchmark suite) into Bemi BIOS v7.2 production firmware deployable on physical x86 hardware.
> **Target**: Bemi BIOS v7.2 for AMD and Intel x86_64 systems, legacy OS support (MS-DOS through Windows 11, Linux), full driver compatibility.

---

## Current State Assessment

| Component | Status | Production Gap |
|---|---|---|
| `bios_prototype.py` | Python simulation of POST + boot flow | Must become real UEFI firmware in C/ASM |
| `hybrid_bemi/` (Rust) | Translates 10 opcodes, executes 5 | Must handle full x86_64 ISA (~1,500 opcodes) |
| Ring -1 DBT | Described in docs only | Must be a real VMX/SVM hypervisor |
| Benchmark suite | Python cycle-count models | Must validate on real silicon |
| CPUID / APIC | Mentioned in TODO.md | Must fully emulate for driver compat |
| Secure Boot | Not implemented | Required for production deployment |
| SMM handling | Open question (res-docs) | Must be resolved for hardware safety |
| Legacy driver support | Not addressed | Critical for production viability |

---

## Phase 0 — Development Infrastructure

### 0.1 Build System & Toolchain
- [ ] Set up EDK2 (EFI Development Kit II) build environment for UEFI DXE driver development
- [ ] Configure cross-compilation: `x86_64-unknown-uefi` target for Rust components
- [ ] Set up NASM/YASM for assembly-level firmware routines (POST, SMM entry)
- [ ] Establish CI/CD pipeline: build → QEMU smoke test → hardware test farm
- [ ] Pin all toolchain versions (LLVM 18+, Rust nightly for `uefi` crate, EDK2 stable tag)

### 0.2 Emulation & Test Harness
- [ ] QEMU/KVM development environment with OVMF (Open Virtual Machine Firmware)
- [ ] Bochs for cycle-accurate x86 instruction-level debugging
- [ ] Intel HAXM / AMD-V nested virtualization for testing VMX/SVM code paths
- [ ] Serial console logging infrastructure (COM1 @ 115200 baud for firmware debug)
- [ ] GDB stub for UEFI-phase debugging via QEMU `-s -S`

### 0.3 Hardware Test Lab
- [ ] Minimum 2 Intel systems (1x consumer desktop, 1x server with VT-x/VT-d)
- [ ] Minimum 2 AMD systems (1x consumer desktop, 1x server with SVM/AMD-Vi)
- [ ] Legacy test systems: 1x BIOS-only (no UEFI), 1x 32-bit-only
- [ ] USB flash drive provisioning for firmware deployment
- [ ] Hardware debugger: Intel DCI or JTAG probe for silicon-level debugging

---

## Phase 1 — UEFI Firmware Foundation

> **Goal**: Replace `bios_prototype.py` with real UEFI firmware that boots on physical hardware.

### 1.1 UEFI DXE Driver Skeleton
- [ ] Create EDK2 package: `BemiBiosPkg/`
- [ ] Implement `BemiBiosCore.efi` — DXE driver loaded during UEFI boot
- [ ] Register UEFI protocols: `EFI_BEMI_PROTOCOL` for runtime services
- [ ] Implement EFI variable storage: `BEMI_NATIVE` (boot mode selector)
- [ ] Memory map: claim firmware-reserved regions for DBT trace cache (mark as `EfiRuntimeServicesData`)

### 1.2 POST (Power-On Self Test) — Real Hardware
- [ ] Detect physical core count via `CPUID` leaf `0x0B` (x2APIC topology)
- [ ] Enumerate L1/L2/L3 cache topology via `CPUID` leaf `0x04`
- [ ] Detect VMX/SVM support: `CPUID.1:ECX[5]` (VMX) or `CPUID.8000_0001:ECX[2]` (SVM)
- [ ] Validate `CR4.VMXE` can be set (Intel) or `EFER.SVME` (AMD)
- [ ] Report detected topology to serial console and UEFI ConOut
- [ ] Calculate BEMI thread count from detected core/cache topology (not hardcoded 144)

### 1.3 Boot Mode Detection
- [ ] Scan GPT/MBR on all block devices via `EFI_BLOCK_IO_PROTOCOL`
- [ ] Detect x86 bootloader: check for `bootx64.efi` (UEFI) or MBR signature `0x55AA`
- [ ] Read `BEMI_NATIVE` EFI variable to determine Mode A vs Mode B
- [ ] If Mode A (native): skip Ring -1 init, chain-load native bootloader
- [ ] If Mode B (legacy): initialize Ring -1 hypervisor before OS handoff

### 1.4 Memory Map Compliance (UEFI 2.10)
- [ ] Construct UEFI memory map with correct `EfiMemoryType` for all regions
- [ ] Reserve trace-cache region: contiguous physical pages marked `EfiRuntimeServicesData`
- [ ] Ensure ACPI tables (RSDP, XSDT, MADT, FADT) are correctly passed to OS
- [ ] Handle `ExitBootServices()` transition: lock Ring -1 state, release boot-time allocations
- [ ] Validate memory map against OS expectations (Windows WHQL, Linux `efi_memmap`)

---

## Phase 2 — Ring -1 Hypervisor Engine

> **Goal**: Build the VMX/SVM hypervisor that intercepts x86 execution beneath the OS.

### 2.1 VMX Root Mode (Intel)
- [ ] `VMXON` region allocation and initialization
- [ ] VMCS (Virtual Machine Control Structure) configuration per logical processor
  - [ ] Guest state: CR0/CR3/CR4, segment registers, GDTR/IDTR, RSP/RIP
  - [ ] Host state: Ring -1 handler entry points, host CR3 (identity-mapped page tables)
  - [ ] Execution controls: intercept `CPUID`, `RDMSR/WRMSR`, `HLT`, `INVLPG`, `MOV CR`
  - [ ] Entry/exit controls: save/restore x87, SSE, AVX state (XSAVE area)
- [ ] VM-exit handler: dispatch table for each exit reason (64+ exit reasons)
- [ ] VPID (Virtual Processor ID) for TLB isolation between guest and host
- [ ] EPT (Extended Page Tables) for guest-physical to host-physical translation
- [ ] Preemption timer for time-slicing Ring -1 maintenance tasks

### 2.2 SVM Root Mode (AMD)
- [ ] VMCB (Virtual Machine Control Block) allocation and initialization
- [ ] `VMRUN` / `VMSAVE` / `VMLOAD` instruction sequences
- [ ] Intercept bitmap: `CPUID`, `MSR`, `IOIO`, `CR` access, `INVLPG`
- [ ] Nested Page Tables (NPT) — AMD equivalent of EPT
- [ ] ASID (Address Space ID) management for TLB isolation
- [ ] `#VMEXIT` handler with AMD-specific exit codes

### 2.3 Unified Abstraction Layer
- [ ] `HypervisorBackend` interface abstracting VMX vs SVM
- [ ] Runtime detection: `if vendor == Intel { vmx_init() } else { svm_init() }`
- [ ] Common VM-exit dispatch: `handle_cpuid()`, `handle_msr()`, `handle_cr_access()`, etc.
- [ ] Per-CPU data structures: one VMCS/VMCB per logical processor
- [ ] IPI (Inter-Processor Interrupt) framework for cross-CPU hypervisor coordination

### 2.4 Trace Cache Infrastructure
- [ ] L3 cache-line locking mechanism (Intel CAT — Cache Allocation Technology)
- [ ] Or: dedicated physical memory region with prefetch hints (fallback for non-CAT CPUs)
- [ ] Trace cache data structure: hash table mapping `(RIP, context)` to translated macro-op sequence
- [ ] Eviction policy: LRU with pinning for kernel hot paths
- [ ] Cache coherency: invalidate entries on guest page-table modifications (`INVLPG` intercept)
- [ ] Size budget: 4 MB default (fits ~100K translated basic blocks)

---

## Phase 3 — Dynamic Binary Translation Pipeline

> **Goal**: Expand `hybrid_bemi/` from 10 opcodes to production-grade x86_64 coverage.

### 3.1 Instruction Decoder Expansion
- [ ] Port `iced-x86` to `no_std` with custom allocator (or replace with embedded decoder)
- [ ] Support all x86_64 instruction prefixes: REX, VEX, EVEX, LOCK, REP/REPZ/REPNZ
- [ ] Decode coverage tiers:
  - **Tier 1 (Critical ~50 opcodes, ~85% of code)**: MOV, ADD, SUB, CMP, JMP, Jcc, CALL, RET, PUSH, POP, LEA, TEST, AND, OR, XOR, SHL, SHR, SAR, IMUL, IDIV, NOT, NEG, INC, DEC, NOP, XCHG, CDQ/CQO, MOVSX/MOVZX
  - **Tier 2 (System ~40 opcodes)**: INT, IRET, SYSCALL, SYSRET, LGDT, LIDT, MOV CRn, RDMSR, WRMSR, CPUID, HLT, CLI, STI, INVLPG, CLFLUSH, MFENCE, XSAVE, XRSTOR, WBINVD
  - **Tier 3 (SIMD ~200 opcodes)**: SSE1-4.2, AVX, AVX2 — macro-op passthrough to host silicon
  - **Tier 4 (Extended ~300 opcodes)**: AVX-512, AES-NI, SHA, BMI1/2, FMA — passthrough
  - **Tier 5 (Legacy ~200 opcodes)**: x87 FPU, MMX, 3DNow!, BCD — software emulation

### 3.2 Translation Engine
- [ ] Basic block detection: scan until unconditional branch, RET, or max 64 instructions
- [ ] RISC micro-op emission with strict 32-bit encoding (existing `ir.rs` format)
- [ ] Register allocation: map x86 registers to BEMI register file (existing `Register` enum)
- [ ] Expand temporary register pool: RTmp0-RTmp7 (current RTmp0-RTmp2 insufficient)
- [ ] Flags emulation: lazy flags evaluation (compute FLAGS only when read)
- [ ] Address mode translation: all 17 x86 addressing modes to Load+ALU sequences
- [ ] Self-modifying code detection: write-protect translated pages, invalidate on write fault

### 3.3 Macro-Op Passthrough Engine
- [ ] Detect SIMD/crypto instructions during translation
- [ ] Emit passthrough macro-ops: 32-bit tokens that route to host ASIC silicon
- [ ] Register mapping: BEMI vector register indices to host XMM/YMM/ZMM registers
- [ ] MXCSR state management: save/restore SSE control register across boundaries
- [ ] AVX-512 opmask passthrough: map k0-k7 registers through translation layer

### 3.4 Optimization Passes
- [ ] Peephole optimization: fuse Load+ALU into single scheduled unit
- [ ] Dead code elimination: remove redundant flag computations
- [ ] Constant folding: pre-compute immediate arithmetic at translation time
- [ ] Branch target linking: patch translated blocks to jump directly to translated targets
- [ ] Hot path profiling: count block execution frequency, re-optimize hot blocks

---

## Phase 4 — Hardware Compatibility Layer

> **Goal**: Make the host OS and drivers believe they are running on native x86 hardware.

### 4.1 CPUID Virtualization
- [ ] Intercept all `CPUID` leaves and return spoofed responses
- [ ] Leaf `0x00`: return genuine vendor string ("GenuineIntel" or "AuthenticAMD")
- [ ] Leaf `0x01`: report correct family/model/stepping from host CPU (pass-through)
- [ ] Feature flags: advertise SSE, SSE2, SSE3, SSSE3, SSE4.1/4.2, AVX, AVX2, AES-NI
- [ ] Conditionally hide: VMX/SVM feature bits (hide hypervisor from guest)
- [ ] Leaf `0x40000000-0x4FFFFFFF`: optionally expose BEMI hypervisor interface
- [ ] AMD-specific leaves: `0x8000_0000` through `0x8000_001F`

### 4.2 MSR (Model-Specific Register) Emulation
- [ ] Shadow MSR bank: intercept `RDMSR`/`WRMSR` for critical MSRs
- [ ] `IA32_EFER` (0xC0000080): manage LME, SCE, NXE bits
- [ ] `IA32_APIC_BASE` (0x1B): shadow APIC base address
- [ ] `IA32_TSC_ADJUST` (0x3B): TSC offset for accurate guest timing
- [ ] `IA32_STAR/LSTAR/CSTAR/SFMASK`: SYSCALL/SYSRET targets
- [ ] Pass-through list: MSRs safe to let the guest access directly

### 4.3 APIC Virtualization
- [ ] Shadow APIC register bank (MMIO at `0xFEE00000` or MSR-based x2APIC)
- [ ] Interrupt injection: translate guest IDT vectors into Ring -1 event injection
- [ ] Timer virtualization: shadow APIC timer with precise TSC-based emulation
- [ ] IPI delivery: guest-to-guest IPIs via hypervisor mediation
- [ ] Hardware-assisted APIC (Intel APICv / AMD AVIC) for reduced VM-exit overhead
- [ ] Legacy PIC (8259A) emulation for ancient OSes (MS-DOS, Windows 9x)
- [ ] I/O APIC emulation: intercept MMIO to I/O APIC region (`0xFEC00000`)

### 4.4 I/O and Device Passthrough
- [ ] PCI/PCIe configuration space passthrough via VT-d/AMD-Vi IOMMU
- [ ] NVMe direct passthrough for storage performance
- [ ] USB controller passthrough (xHCI) for peripheral compatibility
- [ ] GPU passthrough (VFIO-style): assign discrete GPU directly to guest
- [ ] Legacy I/O port emulation: ports `0x60-0x6F` (PS/2), `0x1F0-0x1F7` (IDE), `0x3F8` (COM1)
- [ ] DMA remapping: IOMMU page tables to prevent guest DMA from corrupting host

### 4.5 SMM (System Management Mode) Handling
- [ ] **Approach C** (from `res-docs/04_open_questions/002`): SMM Translation Bypass
- [ ] On SMI: suspend Ring -1 DBT, let SMM code execute natively on x86 silicon
- [ ] Preserve BEMI state (trace cache, TAGE tables) across SMI boundary
- [ ] Protect trace-cache memory from WBINVD issued within SMM handlers
- [ ] Resume Ring -1 DBT after RSM instruction
- [ ] Latency budget: less than 100 us total SMM handling time (thermal safety)

---

## Phase 5 — Legacy OS & Driver Compatibility

> **Goal**: Boot and run legacy operating systems with their native drivers unmodified.

### 5.1 Boot Protocol Support
- [ ] **UEFI boot**: chain-load `bootx64.efi` / `bootia32.efi` within Ring -1 VM
- [ ] **Legacy BIOS boot (CSM)**: implement Compatibility Support Module
  - [ ] Real-mode IVT setup at `0x0000:0x0000`
  - [ ] INT 10h (video), INT 13h (disk), INT 15h (memory map), INT 16h (keyboard)
  - [ ] A20 gate handling
  - [ ] MBR loading at `0x7C00`
- [ ] **Multiboot/Multiboot2**: for Linux direct-boot scenarios
- [ ] **PXE network boot**: TFTP + DHCP for diskless systems

### 5.2 OS-Specific Compatibility
- [ ] **MS-DOS (1.0 - 6.22)**: Real-mode execution within V86 mode under Ring -1
  - [ ] INT 21h trace-cache (existing benchmark model becomes real implementation)
  - [ ] FAT12/FAT16 filesystem passthrough
- [ ] **Windows XP/7/8/10/11**:
  - [ ] HAL compatibility: ACPI tables must be pixel-perfect
  - [ ] WHQL driver signing: CPUID must match real hardware for driver selection
  - [ ] Hyper-V enlightenments: optionally expose to Windows for performance
  - [ ] PatchGuard handling (see Open Questions)
- [ ] **Linux (kernel 2.6+)**:
  - [ ] `dmesg` must show clean boot (no ACPI errors, no IOMMU faults)
  - [ ] KVM coexistence: handle nested virtualization if guest runs KVM
  - [ ] Device tree / ACPI for driver enumeration

### 5.3 Driver Compatibility Matrix
- [ ] **Storage**: NVMe, AHCI (SATA), IDE (legacy), USB mass storage
- [ ] **Network**: Intel e1000/igb, Realtek 8139/8168, Broadcom, Mellanox
- [ ] **GPU**: Intel UHD, AMD Radeon, NVIDIA — via IOMMU passthrough
- [ ] **USB**: xHCI (3.x), EHCI (2.0), OHCI/UHCI (1.x)
- [ ] **Audio**: Intel HDA, USB Audio Class
- [ ] **Input**: PS/2 keyboard/mouse, USB HID
- [ ] **ACPI**: battery, thermal, power management — correct DSDT/SSDT tables

### 5.4 CR3 / Page Table Walk Acceleration
- [ ] Shadow page tables: maintain Ring -1 page tables mirroring guest CR3
- [ ] EPT/NPT auto-sync: hook `MOV CR3` and `INVLPG` to update mappings
- [ ] TLB management: `INVVPID` (Intel) / ASID flush (AMD) on context switch
- [ ] Large page support: 2 MB and 1 GB pages for reduced TLB pressure
- [ ] PCID (Process Context ID) passthrough for OS TLB optimization

---

## Phase 6 — Performance Optimization

> **Goal**: Deliver the theoretical speedups on real hardware.

### 6.1 Trace Cache Warming
- [ ] At boot: pre-translate OS kernel text section into trace cache
- [ ] Adaptive: profile first 10 seconds of execution, pin hot blocks
- [ ] Background: low-priority thread translates cold code ahead of execution
- [ ] Capacity management: evict LRU blocks when cache is full

### 6.2 TAGE Branch Predictor Pre-filling
- [ ] Static analysis of OS kernel at boot: identify branch sites
- [ ] Pre-populate BTB with translated branch targets
- [ ] Indirect branch target cache: pre-fill vtable dispatch targets

### 6.3 Macro-Op Fusion Pipeline
- [ ] Identify fusible instruction pairs during translation (CMP+Jcc, TEST+Jcc)
- [ ] Emit fused macro-ops that occupy single ROB entries
- [ ] Measure actual fusion rate on production workloads (target: 1.3x IPC)

### 6.4 Interrupt Latency Optimization
- [ ] Shadow APIC fast path: handle timer interrupts without full VM-exit
- [ ] Posted interrupts (Intel): inject interrupts without VM-exit
- [ ] Interrupt coalescing: batch low-priority interrupts to reduce exit frequency
- [ ] Target: less than 1 us end-to-end interrupt latency

---

## Phase 7 — Testing & Validation

### 7.1 Automated Test Suite
- [ ] Unit tests for every translated instruction (per-opcode correctness)
- [ ] Integration tests: boot MS-DOS, FreeDOS, Windows XP, Ubuntu in QEMU
- [ ] Regression tests: run existing benchmark suite on real hardware vs models
- [ ] Fuzz testing: random x86 instruction streams through translator
- [ ] Stress tests: 72-hour continuous operation under load

### 7.2 Hardware Validation Matrix

| Test | Intel Consumer | Intel Server | AMD Consumer | AMD Server |
|---|---|---|---|---|
| POST + topology detect | - | - | - | - |
| Ring -1 VMX/SVM init | - | - | - | - |
| CPUID spoofing | - | - | - | - |
| Legacy BIOS boot (DOS) | - | - | - | - |
| UEFI boot (Windows) | - | - | - | - |
| UEFI boot (Linux) | - | - | - | - |
| Driver compatibility | - | - | - | - |
| 72-hour stability | - | - | - | - |

### 7.3 Performance Validation
- [ ] Coremark (integer), STREAM (memory bandwidth), OpenSSL speed (crypto)
- [ ] sysbench (multi-threaded), boot time measurement
- [ ] Energy measurement: wall-plug wattmeter during sustained workloads
- [ ] Latency measurement: interrupt response time, syscall overhead

---

## Phase 8 — Deployment & Packaging

### 8.1 Firmware Image Build
- [ ] Produce flashable UEFI capsule (`.cap` / `.rom`) for supported boards
- [ ] USB installer: bootable USB that installs BEMI as UEFI Option ROM
- [ ] Non-destructive: BEMI installs alongside existing firmware, selectable at boot
- [ ] Recovery: fail-safe boot path that bypasses BEMI if Ring -1 init fails

### 8.2 Configuration Interface
- [ ] UEFI HII (Human Interface Infrastructure) setup screen
- [ ] Options: Enable/Disable BEMI, Boot Mode, Trace Cache Size
- [ ] Debug options: serial logging level, performance counters
- [ ] Runtime hotkeys: Ctrl+Alt+B to toggle BEMI bypass

### 8.3 Update & Recovery
- [ ] UEFI capsule update: signed firmware updates via OS-level tool
- [ ] Version tracking: firmware version in SMBIOS Type 0
- [ ] Rollback: store previous firmware version for recovery

### 8.4 Documentation
- [ ] User manual: installation, configuration, troubleshooting
- [ ] Hardware compatibility list (HCL): tested motherboards and chipsets
- [ ] Developer guide: architecture overview, build instructions
- [ ] API reference: BEMI UEFI protocol specification

---

## Dependency Graph

```
Phase 0 (Infrastructure)
    |
    v
Phase 1 (UEFI Foundation)
    |
    +----------+----------+
    |                     |
    v                     v
Phase 2 (Hypervisor)  Phase 3 (DBT Pipeline)
    |                     |
    +----------+----------+
               |
               v
         Phase 4 (HW Compat)
               |
               v
         Phase 5 (Legacy OS)
               |
         Phase 6 (Performance)  <-- parallel with Phase 5
               |
               v
         Phase 7 (Testing)
               |
               v
         Phase 8 (Deployment)
```

---

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| VMX/SVM differences cause Intel/AMD divergence | High | High | Unified abstraction layer (Phase 2.3) |
| PatchGuard detects Ring -1 on Windows 10/11 | High | Medium | Mimic standard hypervisor signatures |
| SMM WBINVD destroys trace cache | High | Medium | Protect trace region; rebuild on invalidation |
| L1 cache thinning at 144 threads | Medium | Confirmed | Accept as documented trade-off |
| Legacy driver CPUID dependency | Medium | High | Exhaustive CPUID leaf spoofing |
| IOMMU not available on consumer boards | Medium | Medium | Fallback to software I/O interception |
| Self-modifying code in JIT engines | Medium | High | Write-protect + invalidate protocol |
| Interconnect scaling limits (144 threads) | High | Medium | Accept ~100-110 effective threads |

---

## Estimated Timeline

| Phase | Duration | Prerequisites |
|---|---|---|
| Phase 0: Infrastructure | 2 weeks | — |
| Phase 1: UEFI Foundation | 4 weeks | Phase 0 |
| Phase 2: Ring -1 Hypervisor | 8 weeks | Phase 1 |
| Phase 3: DBT Pipeline | 10 weeks | Phase 1 (parallel with P2) |
| Phase 4: HW Compatibility | 6 weeks | Phase 2 + 3 |
| Phase 5: Legacy OS Support | 6 weeks | Phase 4 |
| Phase 6: Performance | 4 weeks | Phase 2 + 3 (partial parallel) |
| Phase 7: Testing | 4 weeks | Phase 5 + 6 |
| Phase 8: Deployment | 3 weeks | Phase 7 |
| **Total (critical path)** | **~35 weeks** | |

---

## Technology Stack

| Component | Language | Rationale |
|---|---|---|
| UEFI DXE driver | C + NASM | EDK2 ecosystem; direct hardware access |
| Ring -1 hypervisor core | C + inline ASM | VMX/SVM requires precise register control |
| DBT translator | Rust (`no_std`) | Memory safety; port existing `hybrid_bemi` |
| DBT executor | Rust (`no_std`) | Same safety guarantees as translator |
| Trace cache | C | Performance-critical; cache-line aligned |
| Benchmark validation | Python | Existing suite; validate models vs hardware |
| Test harness | Python + Bash | QEMU orchestration, serial log parsing |

---

## Open Questions Requiring User Decision

### Q1: Development Target
Should the initial target be QEMU-only (software validation first) or physical hardware from day one? QEMU-first reduces risk but delays real-world validation.

### Q2: Architecture Version
Which Bemi version is the production target? v1.2 (144T, decoder kept) is the only one deployable to **existing** x86 systems without new silicon. v1.1 (36T, decoder removed) requires custom silicon fabrication.

### Q3: Installation Method
Modify existing motherboard firmware (requires OEM cooperation) or run as an Option ROM / UEFI application loaded from disk (no firmware flashing)? The latter is safer and more deployable.

### Q4: Windows Compatibility
Windows PatchGuard actively detects unauthorized Ring -1 code. Options: (a) Microsoft WHQL hypervisor signing, (b) certified Hyper-V compatible hypervisor, or (c) support only Linux + legacy Windows (XP/7).

### Q5: Performance Expectations
Real-world performance will likely be 2-4x (not 7.8x), given interconnect scaling (~100 effective threads vs 144 per res-docs findings) and real cache contention. Is this acceptable?
