# Failed Experiment -- Full Software DBT (Dynamic Binary Translation)

**Status:** ABANDONED -- 2024-10-30
**Duration investigated:** 3 weeks (Oct 2024)
**Published doc reference:** `docs/09_bemi_bios_ring_minus1.md` (mentions DBT but not this failure mode)

---

## What We Tried

Before settling on the hardware-assisted "Ring -1" DBT model, we attempted a **pure software DBT** approach -- a userspace binary translator that intercepts x86 instructions via signal handlers and dynamically recompiles x86 code to native RISC.

## Architecture (Software-Only DBT)

```
Legacy OS binary
       ?
Software DBT (userspace process)
  - Instruction fetch (reads x86 opcodes from memory)
  - Decode (parses variable-length x86 encoding)
  - Translate (maps to native RISC instructions)
  - Emit (writes translated block to code cache)
  - Execute (jumps to translated block)
       ?
Native OS (Linux/macOS)
       ?
x86 hardware
```

## Why It Failed

### Problem 1: Catastrophic Overhead

The software DBT introduces a **translation tax** on every basic block:

| Operation | Cost (cycles) |
|---|---|
| Fetch x86 opcode from memory | ~4 cycles (L1 hit) |
| Parse variable-length encoding | ~50 cycles (software decode) |
| Look up translation in cache | ~10 cycles |
| Emit translated block (first time only) | ~200 cycles |
| Jump to translated code | ~5 cycles |
| **Total per basic block (first execution)** | **~270 cycles** |

Compare to hardware DBT (Ring -1): ~1 cycle translation overhead (hardware parallel decode).

A basic block is typically 5-20 instructions. On first execution, the software DBT spends 270 cycles translating 5-20 instructions that would have taken ~20 cycles natively -- a **13.5x slowdown** on cold code.

Even with a warm translation cache (hit rate >99%), each basic block transition incurs ~15 cycles for lookup and dispatch, compared to ~1 cycle for native execution.

### Problem 2: Signal Handler Hell

We used `SIGSEGV` signal handlers to detect new code pages (a common DBT technique). This required:
- Registering a `sigaction` for `SIGSEGV`
- On each new code page: mprotect the page, translate, then restart
- Handling self-modifying code (invalidate translations on write fault)

Signal handlers in userspace are:
- Slow to invoke (~500 cycles minimum for signal dispatch)
- Non-reentrant (can't handle nested signals)
- Incompatible with threaded applications (which signal handler handles which thread's translation?)

**Result:** Signal-based DBT had non-deterministic performance and crashed on multi-threaded workloads within minutes.

### Problem 3: No Privileged Instruction Support

Userspace DBT cannot intercept:
- `CPUID` (needs Ring 0 to report fake features)
- `CR3` writes (page table changes -- need kernel-level intercept)
- `IN/OUT` port I/O (needs I/O privilege level)
- `HLT`, `LGDT`, `LIDT`, `MOV CR0` (kernel instructions)

Workaround: binary-rewrite these instructions to call back into the DBT runtime. This requires knowing every privileged instruction site in the target binary, which is impossible for dynamically-loaded code.

### Problem 4: Memory Ordering Violations

x86's Total Store Order (TSO) memory model is stricter than ARM's Relaxed model (or RISC-V's). A software DBT must insert memory barriers (DMB instructions on ARM) after every store to maintain TSO semantics on a weakly-ordered host.

For our ARM M1 test system, this required inserting a `DMB.ISH` after every store -- **adding ~40 cycles per store instruction**. Workloads with frequent stores (database transactions, lock-free data structures) slowed by 10-100x.

## Lessons Learned

1. **Software DBT is feasible for legacy emulation** (QEMU user-mode, Rosetta 1) but not for performance.
2. **Hardware support is mandatory** for competitive DBT performance -- specifically:
   - Hardware decode acceleration (parallel instruction boundary detection)
   - Hardware TSO enforcement (shared memory ordering logic)
   - Hardware privilege level management (Ring -1)
3. **The BIOS/Ring -1 approach is correct** for BEMI's goals. The x86 decoder must be kept in silicon and repurposed, not emulated in software.

## Alternative Considered

We also considered hybrid DBT: software translation for cold code, hardware acceleration for hot code. This is what Rosetta 2 does. It works well for ephemeral workloads but adds OS-level complexity and still struggles with kernel-mode translation.

## Current Status

All software DBT code has been purged from the repository. The prototype firmware (`bios_prototype.py`) now assumes a hardware Ring -1 DBT layer, which is the correct architectural choice.

