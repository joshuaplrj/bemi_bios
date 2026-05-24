# Design Rationale -- Why Ring -1 (and not Ring 0 or 3)

**Date:** 2024-11-12
**Published doc reference:** `docs/09_bemi_bios_ring_minus1.md`

---

## The Problem

BEMI's DBT needs to intercept the OS's execution at the instruction level. The question: at what privilege level should this translation layer live?

| Ring | Name | Advantages | Disadvantages |
|---|---|---|---|
| 3 | User mode | Easy to implement | Can't intercept kernel instructions; signal handler overhead; memory ordering issues |
| 0 | Kernel mode (OS) | Can intercept privileged instructions | Requires OS modification; defeats purpose of legacy OS compatibility |
| -1 | Hypervisor | Complete control over guest OS | Hardware virtualization extensions required (VT-x/AMD-V) |
| -2 | SMM | Below the OS and hypervisor | Only triggered by SMI; no general-purpose interception |

## Why Ring -1 (Hypervisor)

### Option A: Ring 3 DBT (Rejected)

This was our first attempt (see `03_failed_experiments/001_full_software_dbt.md`). Failed because:
- Cannot intercept Ring 0 instructions (CPUID, CR3 writes, MSR access)
- Signal handler overhead is catastrophic for frequent intercepts
- Memory ordering (TSO vs weak) requires expensive barriers on every store

### Option B: Ring 0 DBT (Rejected)

A kernel module that performs DBT is technically possible (Linux's `kprobes` + `ftrace` support runtime code patching). Rejected because:

1. **Requires OS kernel modification.** The whole point of BEMI BIOS is to run *unmodified* legacy OSes. If we're modifying the kernel, we've already lost.
2. **The OS can bypass the DBT.** If the DBT is a kernel module, the OS can unload it. If the DBT is inlined into the kernel (kernel patch), then the OS is no longer "legacy."
3. **Single point of failure.** A crash in the DBT takes down the entire OS.

### Option C: Ring -1 (Hypervisor) -- Chosen

Hypervisor-level DBT provides:
- **Complete instruction interception:** The hypervisor (Ring -1) traps all privileged instructions executed by the guest OS (Ring 0). This includes CPUID, CR3, MSR access, IN/OUT, HLT, etc.
- **No OS modification needed.** The legacy OS boots as a guest VM and never knows it's being translated.
- **Hardware acceleration.** Modern x86 CPUs have hardware virtualization extensions (VT-x, AMD-V) that handle the intercept path in ~50 cycles -- far faster than software signal handlers.
- **Memory isolation.** The DBT's translation cache lives in hypervisor-private memory, invisible to the guest OS.

### Option D: Ring -2 (SMM) -- Not Viable

SMM is too restricted:
- Can only be entered via SMI (System Management Interrupt)
- SMM code runs from SMRAM, which is fixed-size (typically 128KB-1MB)
- Cannot intercept normal OS execution -- only asynchronous events
- Used for power management and thermal control, not general-purpose computation

## But Wait -- BEMI Uses Firmware-Level DBT, Not VT-x

The published docs claim BEMI implements DBT "directly in firmware at Ring -1" -- not using Intel VT-x/AMD-V. This is a subtle but important distinction:

| Feature | VT-x DBT | BEMI Firmware DBT |
|---|---|---|
| Entry/exit cost | ~50 cycles (VM entry/exit) | ~1 cycle (firmware-level intercept table) |
| Transparency | Guest detects VM (timing side channels) | Guest cannot detect intercept |
| Memory isolation | EPT (Extended Page Tables) | Custom TLB slicing |
| Priority | Below hypervisor, above OS | Below hypervisor, above OS (same) |
| Portability | x86-only | Any RISC back-end |

VT-x was designed for *virtualization* (running a complete OS with minimal intercepts). BEMI needs *translation* (intercepting every instruction). VT-x's VM entry/exit cost (50 cycles per intercept) would be prohibitive for instruction-level DBT.

BEMI's approach is closer to a hardware-accelerated QEMU: the firmware has a dedicated translation lookaside buffer that maps x86 instruction addresses to pre-translated RISC code blocks. The intercept is a simple TLB miss, not a full VM exit.

## Implementation Sketch (from patent research)

```
x86 instruction fetch by guest OS
       ?
Firmware TLB lookup (Ring -1):
  - Hit: redirect to cached RISC translation block
  - Miss: invoke DBT engine to translate and cache
       ?
RISC translation block executes on back-end
       ?
On exception/interrupt/syscall:
  - Shadow IDT intercepts -> routes to DBT's emulated IDT
  - DBT emulates the interrupt in software
  - Returns to translated code
```

The key innovation vs VT-x: the intercept is cycle-level (TLB lookup), not context-switch-level (VM exit/entry).

## Cost of Ring -1 Approach

1. **Firmware complexity.** The DBT engine runs permanently in Ring -1. It cannot be paged out. It must handle every x86 instruction correctly. Bugs crash the entire system.
2. **Shadow state management.** The DBT maintains shadow copies of CR3, IDT, GDT, LDT, APIC, and MSRs. Every guest write to these structures must be intercepted and shadowed.
3. **Self-modifying code.** If the guest OS modifies its own code (JIT compilers, binary patching), the DBT must invalidate the corresponding translations. This requires write-protecting the guest's code pages -- a complexity similar to a garbage collector's write barrier.

## Conclusion

Ring -1 is the correct choice. The implementation difficulty is high but the architectural benefits (full interception, no OS modification, hardware acceleration) justify it. The BEMI firmware DBT approach is superior to VT-x for the specific use case of instruction-level translation, though it remains untested in silicon.

