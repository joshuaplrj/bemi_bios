# Open Question -- System Management Mode (SMM) Sandboxing

**Filed:** 2024-11-06
**Priority:** High -- blocks legacy OS compatibility claim
**Published doc reference:** `TODO.md` mentions SMM in Phase 1

---

## The Problem

x86 processors have a **System Management Mode (SMM)** -- the most privileged execution mode (Ring -2). SMM is entered via a System Management Interrupt (SMI) and runs firmware code from SMRAM (a protected memory region invisible to the OS).

**The challenge:** SMM is hidden from and unsynchronized with the OS. An SMI can fire at any instruction boundary, pause the OS, execute arbitrary SMM code, and return -- all without the OS knowing.

## Why This Matters for BEMI

In BEMI's Legacy OS mode, the Ring -1 DBT translator maintains the illusion of a native x86 CPU. If an SMI fires:

1. The hardware SMM handler expects to run on an x86 CPU with x86 register state
2. SMM code may read/write x86-specific MSRs, I/O ports, and memory-mapped configuration space
3. SMM may use instructions like `RSM` (return from SMM) that have specific x86 semantics
4. The OS may use `SMI#` to enter SMM for power management (ACPI), thermal throttling, or firmware updates

BEMI's DBT must:
- Intercept SMI and switch the execution context to x86-native mode (or an SMM-aware translation mode)
- Preserve BEMI's internal state (macro-op cache, TAGE tables, etc.) across the SMI boundary
- Return cleanly via `RSM` without corrupting state

## Specific Unknowns

### Unknown 1: SMRAM Layout

- x86 SMRAM starts at a physical address set by the chipset (usually `0x30000` or `0xA0000`)
- BEMI's Ring -1 DBT uses L3 cache for its translation matrix
- Does SMRAM occupy L3 cache space? Does the DBT's translation matrix survive an SMI entry?
- If SMM code does a WBINVD (write-back invalidate), it flushes all caches -- killing the DBT's cached translations

**Risk:** WBINVD in SMM -> DBT translation cache destroyed -> legacy OS resumes on cold cache -> 100ms+ stall while retranslating.

### Unknown 2: SMM Code Uses DBT-Unfriendly Instructions

SMM code is written by BIOS vendors and uses instructions that are hard to translate:

| Instruction | Problem |
|---|---|
| `RSM` | Returns from SMM; must atomically restore all hidden x86 state |
| `IOUT` / `IIN` | Direct I/O port access (SMM uses this for chipset control) |
| `MSR reads/writes` | SMM often accesses MSRs not exposed to the OS (e.g., thermal, power management) |
| `GETSEC` | Intel TXT/SMX instructions for measured boot |
| Self-modifying SMRAM | SMM can modify its own code in SMRAM |

**Risk:** These instructions are fundamentally x86-specific and may not have RISC equivalents. The DBT may need to fall back to x86-native execution for SMM handlers -- meaning the chip must include a full x86 execution core just for SMM.

### Unknown 3: Latency Requirement

SMM handlers must complete within strict latency bounds (typically <100 ?s for thermal throttling). If BEMI's DBT adds even 1 ?s of translation overhead per SMM instruction, it could violate thermal constraints and damage hardware.

## Proposed Approaches

### Approach A: Native SMM Core (Reserved x86 Silicon)

Keep a single minimal x86 execution core on the die specifically for SMM. This is brute-force but safe. Cost: ~10-15% of die area for a minimal x86 core (no L3 cache, simple decoder, narrow back-end).

**Pros:** No translation latency; no correctness concerns; uses existing UEFI SMM firmware as-is.

**Cons:** Adds silicon cost; only useful during SMM (which is <1% of runtime); the die area could otherwise hold ~10 RISC back-end units.

### Approach B: SMM-Aware DBT

Extend the DBT to recognize SMM entry (via SMI) and pre-translate the entire SMRAM region at boot time. Since SMM code is fixed (loaded from SPI flash into SMRAM at boot), one-time pre-translation is feasible.

**Pros:** No extra silicon; transparent to the legacy OS.

**Cons:** Cannot handle self-modifying SMM code (rare but it exists); DBT must handle `RSM` correctly; WBINVD still invalidates the translation cache.

### Approach C: SMM Translation Bypass

When an SMI fires, the DBT stops translating and directly executes x86 instructions on the shared silicon back-end (which is x86-native in Weaponized mode). After `RSM`, the DBT resumes.

**Pros:** No SMM translation overhead; works with unmodified SMM firmware.

**Cons:** During SMM, BEMI's advantages (thread density, fusion) are unavailable. The OS is paused anyway, so this may be acceptable.

## Recommendation (Preliminary)

Approach C is the simplest and safest. In Weaponized mode (v1.2), the silicon is x86-native on the back-end -- the DBT is a front-end transformation layer. During SMM, we bypass the DBT entirely and execute directly on the native x86 back-end. The cost is that BEMI's 144-thread density is unavailable for the duration of the SMI -- but since the OS is paused during SMM, this doesn't matter.

**Unresolved question:** In native BEMI mode (no x86 back-end), Approach A is the only option. Does BEMI v1.1 (pure RISC die) include SMM support?

## Current Status

**Blocked.** No hardware to test against. The planned FPGA prototype will need to simulate SMM behavior to validate the bypass approach.

