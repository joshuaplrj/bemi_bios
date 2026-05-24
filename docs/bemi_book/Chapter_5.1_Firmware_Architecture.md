# Chapter 5: Implementation of the Bemi BIOS (pro-tes)

## 5.1 Firmware Architecture (EDK2 Integration)

### 5.1.1 The UEFI Foundation
To deploy the Bemi BIOS (`pro-tes`) as a functional Ring -1 hypervisor on modern, commercially available Intel and AMD motherboards, the firmware must integrate seamlessly with the Unified Extensible Firmware Interface (UEFI). The days of legacy 16-bit BIOS interrupts (e.g., `INT 13h`) are entirely obsolete.

Modern firmware development is standardized around the **EFI Development Kit II (EDK2)**, an open-source project maintained by the TianoCore community. EDK2 provides a massive, C-based codebase that handles the agonizingly complex hardware initialization phases: Security (SEC), Pre-EFI Initialization (PEI), and the Driver Execution Environment (DXE).

The Bemi firmware does not reinvent these phases. Attempting to write custom DDR5 memory training algorithms or PCIe root complex enumerators would tie the project to a single specific motherboard model. Instead, the Bemi BIOS is packaged as a **UEFI DXE Application** (a standard `.efi` executable). 

By executing in the DXE phase, the Bemi firmware inherits a fully initialized hardware environment. System RAM is mapped, the APIC is active, and basic video output (GOP) is available. 

### 5.1.2 The Bemi Boot Sequence
When the motherboard's native UEFI Boot Manager selects the Bemi `bemi_bootx64.efi` application, the system transitions from native hardware initialization into the Bemi Hypervisor initialization pipeline.

The architecture is divided into three distinct phases:
1. **The C-based EDK2 Wrapper:** A minimal C program that conforms to the `UefiMain` entry point required by the UEFI specification.
2. **The Hypervisor Initialization Layer:** Written in C, this layer configures the CPU control registers (`CR0`, `CR4`, `IA32_EFER`) to enable VT-x/AMD-V virtualization, allocates the massive physical memory blocks required for the Translation Cache, and constructs the Virtual Machine Control Structure (VMCS).
3. **The Rust DBT Engine:** The core of the Bemi optimization intelligence, written entirely in `#![no_std]` Rust, which performs the JIT translation and macro-op fusion.

### 5.1.3 Allocating the Translation Cache
As discussed in Section 2.4.2, the Bemi firmware must hide gigabytes of physical RAM from the guest Operating System to store the JIT-compiled binaries. 

This must be done via the UEFI memory map *before* the Bemi hypervisor launches the OS. The UEFI specification provides the `AllocatePages` boot service. 

**Algorithm 5.1.1: Securing the Translation Cache (C/EDK2)**
```c
// C-based EDK2 implementation to secure physical RAM for the Translation Cache
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

#define BEMI_TC_SIZE_PAGES 524288 // 2 Gigabytes (524,288 * 4KB pages)

EFI_PHYSICAL_ADDRESS AllocateTranslationCache() {
    EFI_PHYSICAL_ADDRESS tc_base_address = 0;
    EFI_STATUS status;

    // We request the memory as EfiReservedMemoryType.
    // When the Guest OS (Windows/Linux) reads the UEFI memory map during boot,
    // it will see this 2GB block as "Reserved" and will never attempt to 
    // read, write, or map it. It is effectively invisible to Ring 0.
    status = gBS->AllocatePages(
        AllocateAnyPages,
        EfiReservedMemoryType, 
        BEMI_TC_SIZE_PAGES,
        &tc_base_address
    );

    if (EFI_ERROR(status)) {
        Print(L"CRITICAL ERROR: Failed to allocate Bemi Translation Cache!\n");
        CpuDeadLoop(); // Halt the physical processor
    }

    // Zero out the 2GB block to ensure deterministic JIT execution
    ZeroMem((VOID*)tc_base_address, BEMI_TC_SIZE_PAGES * EFI_PAGE_SIZE);

    return tc_base_address;
}
```

By utilizing `EfiReservedMemoryType`, the Bemi C wrapper guarantees that the OS will not corrupt the Translation Cache. Once this memory is secured, the C code passes the physical pointer (`tc_base_address`) across the Foreign Function Interface (FFI) boundary to the Rust DBT engine, allowing the JIT compiler to begin initializing its internal hash maps and DAG allocators.
