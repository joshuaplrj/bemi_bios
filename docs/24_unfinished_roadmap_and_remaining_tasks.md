# 24. Unfinished Roadmap & Remaining Tasks

This document consolidates and details the unfinished engineering tasks, open microarchitectural questions, and pending codebase fixes required to transition the Bemi BIOS from a high-fidelity simulation and prototyping phase to a production-grade firmware release.

---

## 1. Unfinished Production Features (Roadmap)

While the core Bemi hypervisor, DBT pipeline, and CSM logic are functionally validated in simulation, the following implementation details from [TODO.md](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/bemi_bios/TODO.md) remain unfinished for full hardware compatibility and deployment.

### Phase 4: Hardware Compatibility (CPUID, MSR, APIC)
*   **CPUID Leaf Expansion**: Support for standard leaves `0x00-0x1F`, AMD extended leaves `0x80000000-0x8000001F`, and the custom Bemi hypervisor interface (`0x40000000-0x4FFFFFFF`).
*   **MSR Shadowing**: Complete shadowing for Memory Type Range Registers (MTRRs, `0x200-0x2FF`), Page Attribute Table (PAT, `0x277`), debug MSRs (`0x1D9`), and performance counters (`0x186`, `0xC1`).
*   **Interrupt & APIC Emulation**: Connecting local APIC shadow writes to hypervisor interrupt injection, implementing the TSC deadline timer, and completing the 8259A PIC cascade and I/O APIC MMIO emulation.

### Phase 5: DBT Pipeline (Rust DBT Compiler)
*   **Complete x86 Opcode Decoding**: Full decoding coverage for all primary opcodes, two-byte escapes (`0x0F`), three-byte maps (`0x0F38`/`0x0F3A`), and AVX/AVX-512 extensions (VEX and EVEX encodings).
*   **Register Allocator & Cache**: Implementation of a linear-scan register allocator, executable code caching with flush mechanisms, exception table mapping, and cache coherency (CLFLUSH).
*   **Self-Modifying Code (SMC)**: Support for tracking guest writes to translated code pages, write-protecting pages, and invalidating cached code blocks dynamically.

### Phase 6 & 7: CSM, ACPI, SMBIOS & Boot
*   **Legacy CSM OS Boot**: Implementing real-mode switches (64-bit to 16-bit real-mode), reading MBR, and booting legacy OSes (FreeDOS, MS-DOS) with BIOS keyboard, timer, and disk interrupts.
*   **ACPI & SMBIOS Tables**: Proper dynamic population and installation of ACPI (RSDP, XSDT, MADT, FADT, DSDT, HPET) and SMBIOS (Type 0, 1, 4, 7, 16) tables into UEFI configuration tables.
*   **OS Handoff**: Intercepting page table base updates (`MOV CR3`) to update Extended Page Tables (EPT) synchronously, allowing a standard Linux kernel to boot to serial output.

### Phase 8 & 9: Testing & Deployment
*   **QEMU Integration Tests**: Automated round-trip verification of guest payloads (HLT, CPUID, IO, MSR) and a 72-hour soak/stress test.
*   **USB Installer & Capsule Updates**: Bootable USB installer creation, UEFI signed capsule updates, and coreboot payload integration.

---

## 2. Active Codebase Issues & Technical Debt

The issues detailed in [issue_audit.md](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/bemi_bios/docs/issue_audit.md) represent immediate technical debt that must be addressed to ensure clean compilation and standard testing.

### Test Harness & Pytest Discovery
*   **Non-Standard Filenames**: Test scripts (e.g. `BootFreeDOS.py`) do not use the `test_*.py` pattern required for automatic pytest discovery.
*   **Missing Built Payloads**: Standalone tests reference compiled payloads (like `QemuRoundTrip.efi` or `BemiBiosCore.efi`) using paths that are either missing in the workspace or mismatch between build output directories.

### Portability & Build System Assumptions
*   **Linux-Only Makefiles**: The build makefiles and testing shell scripts rely heavily on Linux shell utilities (`find`, `nproc`, `dd`) and hardcoded host paths (`/usr/share/ovmf/OVMF*.fd`), preventing them from running natively on Windows development environments.

### Rust DBT Compilation Warnings
*   **Ambiguous Glob Re-exports**: `lib.rs` has overlapping glob re-exports for `MAX_CODE_SIZE` from both the `codegen` and `ir` modules.
*   **Contiguous Range Warnings**: The x8088 decoder uses non-contiguous range endpoints (`0xD0..0xD3`) which should be updated to inclusive ranges.
*   **Unused Imports**: Multiple files under `dbt/src/x8088/` contain unused imports and dead variable declarations.

### Missing Dependency Management
*   **Python requirements**: Benchmark scripts require third-party libraries (`pandas`, `numpy`, `matplotlib`) but the repo lacks a `requirements.txt` or `pyproject.toml` configuration to set up a developer environment.

---

## 3. Open Microarchitectural Questions

As highlighted in [13_conclusion_and_future_work.md](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/bemi_bios/docs/13_conclusion_and_future_work.md), the Bemi v7.2 architecture relies on several theoretical assumptions that require real hardware validation:

1.  **ROB Density Multiplier**: While simulation models a 3x ROB density increase from reclaiming the CISC decoder complex, physical silicon realization must verify interconnect fanout, voltage distribution, and thermal density limits.
2.  **Macro-Op Fusion Rates**: The 1.3x fusion factor depends heavily on compiler optimization quality, register allocation, and the quality of recompiled binaries.
3.  **TDP and Power Savings**: The 65W/85W TDP estimates assume predictable power scaling, which must be validated on physical FPGAs or tape-out silicon under heavy workloads.
4.  **Register File Sizing**: Determining the optimal physical register count (64 virtual registers) to prevent register spills in vector-heavy code without incurring excessive die area overhead.
