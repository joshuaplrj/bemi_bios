# BEMI BIOS — Production Deployment Plan
## Deploying to Real x86 (AMD + Intel) Systems with Full Legacy Compatibility

> **Scope**: Transform the current prototype (`bios_prototype.py`, `hybrid_bemi/`, benchmark suite) into production firmware deployable on physical x86 hardware.
> **Target**: AMD and Intel x86_64 systems, legacy OS support (MS-DOS through Windows 11, Linux), full driver compatibility.

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
- [ ] Set up NA
<truncated 8814 bytes>
E3, SSE4.1/4.2, AVX, AVX2, AES-NI
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
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.