# Reference Notes -- Prior Art in Dynamic Binary Translation

**Date:** 2024-10-20
**Purpose:** Survey existing DBT systems to validate BEMI's approach and identify failure modes

---

## Surveyed Systems

| System | Source -> Target | Year | Approach | Performance |
|---|---|---|---|---|
| QEMU (user mode) | Any -> TCG IR -> Any | 2005 | Basic block JIT, software | ~10-20% native |
| QEMU (full system) | Any -> TCG IR -> Any | 2005 | Full system emulation, software | ~5-10% native |
| Rosetta (PowerPC -> x86) | PPC -> x86 | 2006 | Software DBT + cached translations | ~50-80% native |
| Rosetta 2 (x86 -> ARM) | x86_64 -> ARM64 | 2020 | Hybrid: SW + hardware accelerated | ~80-100% native |
| IA-32 EL (Itanium) | x86 -> IA-64 | 2001 | Hardware DBT in silicon | ~80% native |
| Transmeta Crusoe | x86 -> VLIW | 2000 | Hardware DBT + code morphing ROM | ~50-70% native |
| NVidia Denver | ARM -> custom | 2014 | DBT + static optimization | ~90% native (reported) |
| Apple M1 (Rosetta 2 HW assist) | x86_64 -> ARM64 | 2020 | DBT + hardware TSO + L3 cache | ~95% native (SPEC) |

---

## Key Lessons

### 1. Hardware DBT Consistently Outperforms Software DBT

Every pure-software DBT system (QEMU, early Rosetta) achieves 10-50% of native performance. Every hardware-assisted DBT system (IA-32 EL, Transmeta, Rosetta 2, NVidia Denver) achieves 70-100%.

**BEMI implication:** The Ring -1 hardware DBT approach is correct. A software-only BEMI translator would be nonviable (confirmed by our own failed experiment).

### 2. Transmeta's Failure Is Instructive

Transmeta's Crusoe (2000) was the closest prior art to BEMI:
- x86 -> VLIW binary translation at runtime
- Hardware DBT in the "Code Morphing" layer (firmware-level, below the OS)
- Claimed 50-70% of native x86 performance

**Why Transmeta failed:**
a) **VLIW scheduling is hard.** Transmeta's VLIW compiler could not find enough instruction-level parallelism in x86 binaries. The VLIW bundles were often half-empty.
b) **Wrong market timing.** They launched in 2000 (low-power mobile computing wasn't mainstream) and couldn't compete with Intel's clock speed race.
c) **No x86 compatibility guarantee.** Some x86 instructions (especially FPU and MMX) had correctness bugs in translation that took years to fix.
d) **Silicon was mediocre.** The Crusoe was built on a trailing-edge process (180nm vs Intel's 130nm).

**BEMI lesson:** Market timing and execution quality matter as much as architecture. Having a better DBT isn't enough -- you need a competitive process node and the resources to fix every x86 compatibility bug.

### 3. Rosetta 2's Secret Sauce

Rosetta 2 achieves ~95% native performance through:

a) **Hardware TSO.** Apple's M1 chip includes hardware Total Store Order support, so x86 memory ordering can be emulated without inserting software barriers.
b) **L3 cache for translations.** The M1 uses a portion of the L3 cache as a translation cache. Translations survive context switches (unlike software caches in QEMU).
c) **Pre-translation at install time.** Rosetta 2 translates x86 binaries in bulk at install time (or first launch), not just-in-time. This eliminates the cold-start translation penalty.
d) **Narrow scope.** Rosetta 2 only translates user-mode x86_64 code. No kernel mode, no legacy 32-bit x86, no real-mode, no V8086.

**BEMI implications:**
- Hardware TSO is essential for DBT performance on weakly-ordered back-ends
- L3 cache is the right place for the translation cache (BEMI already assumes this: "DBT locked into L3 cache")
- Pre-translation at boot time (not first use) reduces warm-up penalty
- BEMI's Ring -1 approach goes further than Rosetta 2 (supports kernel mode and legacy x86)

### 4. The Performance Wall at ~95%

No DBT system has achieved >95% of native performance on arbitrary code. The remaining 5% comes from:
- **Residual decode overhead** (even hardware DBT adds a few cycles per basic block for the initial fetch)
- **Translation cache misses** (on context switches, the cache must warm up)
- **Microcoded instructions** (CPUID, XSAVE, REP MOVS -- must be interpreted, not translated)
- **Self-modifying code** (triggers cache invalidation and retranslation)

**BEMI verification:** The published docs claim "zero OS degradation" and "negative latency overhead." This is inconsistent with the 95% wall observed in every real DBT system. The "negative latency" claim appears to be aspirational rather than evidence-based. This should be revised to ">95% of native performance" in future publications.

---

## Summary Table

| Feature | BEMI Claim | Prior Art Best | Feasibility |
|---|---|---|---|
| DBT performance | >100% native | ~95% (Rosetta 2) | Unlikely -- revise to >95% |
| Kernel mode DBT | Supported | None (Rosetta 2, Transmeta) | Novel -- unproven |
| Pre-boot translation | Boot-time | Install-time (Rosetta 2) | Boot-time is harder but possible |
| Hardware TSO | Yes (native) | Yes (Apple M1) | Confirmed necessary |
| Thread density | 144 threads | 24 (x86 native) | Unproven -- depends on silicon |
| Single-core IPC | 1.3x (v1.2) | 1.0x (x86 baseline) | Modest -- achievable |
| Power | 85W vs 100W | 85W vs 100W | Plausible |

---

## References

- Transmeta Crusoe papers: "The Technology Behind Crusoe" (Klaiber, 2000)
- Rosetta 2 analysis: "Running x86 Software on ARM" (Apple WWDC 2020)
- QEMU internals: "QEMU: A Multithreaded Emulator" (Bellard, 2005)
- NVidia Denver: "Denver: A High-Performance x86-Compatible ARM Processor" (Hot Chips 2014)

