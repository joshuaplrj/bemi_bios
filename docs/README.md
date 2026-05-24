# Bemi BIOS -- Complete Technical Documentation

This folder contains the definitive, end-to-end technical documentation for the **Bemi BIOS** project.
It covers the full engineering arc: from the original x86 problem statement, through every architectural
iteration, to the final honest benchmark suite validated against **MS-DOS 1.0** as a real legacy OS workload.

---

## Architecture Version Summary

| Property | x86 (Baseline) | Bemi v1.1 | Bemi v1.2 (HW spec) | Bemi v1.3 (ROB Density) | Bemi v2.0 (Scaled Dom.) | Bemi v3.0 (Mem & Pred) | Bemi v4.0 (Zenith) |
|---|---|---|---|---|---|---|---|
| Implementation | Silicon native | Compiler-native ISA | x86 decoder kept | ROB density (4B entry) | L0, independent ROB, MLP | 128MB L4, HMC, PTC | Adaptive HMC, 256MB L4, NPP, DCF |
| Physical Cores | 12 | 12 | 12 | 12 (host) | 12 | 12 | 12 |
| Virtual Threads | 24 | **36** (3x ROB density) | **144** (6nm RISC size) | **84** (24 x 3.5 density) | **48** (SMT-4) | **60** (SMT-5) | **72** (SMT-6 / 36 Fused) |
| Decode Latency | 4 cyc | **1 cyc** (fixed-32) | 4 cyc (decoder KEPT) | 4 cyc (decoder kept) | 4 cyc (decoder kept) | 1.75 cyc (blended PTC) | 1.35 cyc (blended NPP) |
| IPC / thread (peak) | 1.0x | **5.2x** (decode + fusion) | 1.3x (fusion only) | 1.3x (fusion only) | 1.5x (6-pair fusion) | 3.66x (8-pair fusion) | 5.18x (10-pair fusion) |
| Total Throughput | 24.0 | 187.2 | 187.2 | 109.2 (1.3 x 84) | 72.0 | 219.6 | 373.3 (SMT) / 186.7 (Fused) |
| TDP | 100 W | **65 W** (decoder removed) | 85 W (decoder kept) | 80 W | 75 W | 85 W | 90 W |
| L1 / thread (raw) | 16.0 KB | 10.7 KB | 2.67 KB | 4.57 KB | 8.0 KB | 6.4 KB | 5.3 KB (SMT) / 10.7 KB (Fused) |
| Single-core vs x86 | 1.0x | **5.2x** | 1.3x | 1.3x | 1.5x | 3.66x | 5.18x (SMT) / 10.37x (Fused) |
| Multi-core vs x86 | 1.0x | **7.8x** | **7.8x** | **4.55x** | 1.98x (grounded avg) | 4.83x (grounded avg) | 6.75x (grounded avg) |
| Key weakness | Baseline | Fewer threads (36) | Low L1/thread (2.67 KB) | Lower thread count vs v1.2 | BW Gov limits on Mem-heavy | 85W TDP; serial limits | Higher area footprint; 90W TDP |

> **Note:** v1.2 is the hardware architecture specification. v1.3 (ROB Entry Density) uses 4-byte
> ROB entries (vs x86's 14 bytes) to achieve 3.5x more entries per SRAM byte without additional die area.
> Versions v2.0, v3.0, and v4.0 introduce physical improvements (L0 micro-caches, 3D V-Cache, memory compression, branch predictors, dynamic core fusion) to eliminate cache thrashing and memory bottlenecks.
> All constants come from `bemi_constants.py`.

---

## How to Read This Documentation

The documents are numbered in **chronological order** -- each builds directly on the previous chapter.

---

## Document Index

| # | Document | What it covers |
|---|---|---|
| 01 | [The x86 Problem & The Bemi Hypothesis](01_x86_problem_and_hypothesis.md) | The original RISC hypothesis; 3x density multiplier; mathematical framework |
| 02 | [CISC Instruction Taxonomy](02_cisc_instruction_taxonomy.md) | Five x86 instruction categories; per-category Bemi strategy |
| 03 | [Bemi v1.0 -- Hybrid DBT](03_hybrid_bemi_first_implementation.md) | Rust translator; 32-bit MicroOp encoding; 2.0x expansion factor |
| 04 | [Micro-Op Deep Dive](04_micro_op_deep_dive.md) | x86 uop anatomy; ROB structure; why fixed-32 is denser |
| 05 | [CISC Dominance Problem](05_cisc_dominance_problem.md) | Where pure RISC fails; AVX-512 (5.4x), AES-NI (10x) losses |
| 06 | [Macro-Op Hardware Passthrough](06_macro_op_passthrough_breakthrough.md) | The passthrough breakthrough; architecture inversion; 3-cycle saved |
| 07 | [Bemi v1.1 -- Native ISA Evolution](07_native_isa_evolution.md) | TSO, SMC, indirect branch DBT failures; compiler co-design solution |
| 08 | [Bemi v1.2 -- Weaponized x86 Bemi](08_weaponized_x86_bemi.md) | 6nm thread count derivation; x86 decoder kept; 144-thread model |
| 09 | [The Bemi BIOS -- Ring -1 Firmware](09_bemi_bios_ring_minus1.md) | Boot modes; MS-DOS 1.0 trace-cache analysis; 59.43x speedup |
| 10 | [Benchmark Integrity Analysis](10_benchmark_integrity_analysis.md) | The five cheats; corrections; what the original code got wrong |
| 11 | [Honest Benchmark Methodology](11_honest_benchmark_methodology.md) | Ground-truth constants for v1.1 and v1.2; IPC formula; cache model |
| 12 | [Full Benchmark Results](12_full_benchmark_results.md) | All 12 benchmark results; win/loss analysis (v1.2 model) |
| 13 | [Conclusion & Future Work](13_conclusion_and_future_work.md) | What has been proven; roadmap; open engineering questions |
| 14 | [Version Comparison: v1.1 vs v1.2 vs v1.3](14_architecture_version_comparison.md) | Side-by-side benchmark results across all four architectures |
| 15 | [Bemi v2.0 -- Scaled Dominance](15_v20_scaled_dominance.md) | L0 micro-cache; independent ROB; MLP latency; BW governor; zero regressions |
| 16 | [Bemi v3.0 -- Memory & Predictor Ascendancy](16_v30_ascendancy.md) | L4 stacked cache (128MB); Hardware Memory Compression (HMC); PTC trace cache |
| 17 | [Bemi v4.0 -- Ultra-Bandwidth & Execution Zenith](17_v40_zenith.md) | Adaptive HMC; 256MB L4 cache; Neural Perceptron predictor; Dynamic Core Fusion |

---

## Quick Reference: Constants by Version

### Bemi v1.1 -- Native RISC ISA (ROB Density Model)
```
Physical cores  : 12
Virtual threads : 36   (12 cores x 3x ROB density, freed by removing decoder)
Decode latency  : 1 cycle  (fixed-32 decoder -- no variable-length scanning)
IPC / thread    : (4/1) x 1.3 = 5.2
Total TP        : 5.2 x 36 = 187.2
TDP             : 65 W  (x86 decoder complex fully removed)
L1 / thread     : (12 x 32 KB) / 36 = 10.67 KB
INT 21h cost    : 8 cycles (Ring -1 trace cache hit)
Strength        : Single-thread latency (5.2x IPC), power efficiency (65W)
Weakness        : Fewer threads (36); smaller silicon counts don't apply
```

### Bemi v1.2 -- Weaponized x86 Bemi (6nm Physical Model)
```
Physical cores  : 12
Virtual threads : 144  (12 decoder clusters x 15 RISC units x 0.85 overhead)
                       Derived: RISC back-end 0.15 mm? vs x86 back-end 2.25 mm? at 6nm
Decode latency  : 4 cycles  (x86 decoder KEPT, weaponized for macro-op fusion)
IPC / thread    : (4/4) x 1.3 = 1.3
Total TP        : 1.3 x 144 = 187.2
TDP             : 85 W  (decoder kept, RISC back-ends more efficient)
L1 / thread     : (12 x 32 KB) / 144 = 2.67 KB
INT 21h cost    : 8 cycles (Ring -1 trace cache hit)
Strength        : Throughput (144 threads), keeps x86 HW ecosystem intact
Weakness        : Single-thread = only 1.3x vs x86; severe L1 cache thinning
```

### Bemi v1.3 -- ROB Entry Density Update (ROB Size Reduction)
```
Physical cores  : 12
Virtual threads : 84   (24 x 3.5 ROB density -- 4B entries vs x86 14B)
Decode latency  : 4 cycles  (x86 decoder kept for fusion)
IPC / thread    : (4/4) x 1.3 = 1.3
Total TP        : 1.3 x 84 = 109.2
TDP             : 80 W
L1 / thread     : (12 x 32 KB) / 84 = 4.57 KB
INT 21h cost    : 8 cycles (Ring -1 trace cache hit)
Strength        : 3.5x ROB density from same SRAM budget; no CAM O(n^2) penalty
Weakness        : Fewer threads than v1.2 (84 vs 144); still needs x86 decoder for fusion
```

---

## How to Run the Benchmarks

All benchmark and comparison scripts should be executed from the **repository root folder**:

```bash
# Full benchmark suite (includes x86 vs v1.2, v1.3, v2.0, v3.0, and v4.0 summary):
python bemi_bios/run_all_benchmarks.py

# Three-way comparison (x86 vs v1.1 vs v1.2):
python compare_all_three.py

# v1.3 ROB Entry Density parameter sweeps and workload simulation:
python tests/rob_density_benchmark.py
python tests/rob_dbt_benchmarks.py

# Grounded Model Scaling Progression (x86 vs v1.3 Grounded vs v2.0 vs v3.0 vs v4.0):
python tests/scaling_bottlenecks_test.py

# Run the complete test suite (includes all individual tests):
python run_all_tests.py
```

---

## Final Scores

### v1.3 ROB Entry Density (84-thread model)

| Metric | Value |
|---|---|
| ROB entry size | 4 bytes (vs x86 14 bytes; 3.5x density) |
| Virtual threads | 84 (24 baseline x 3.5 density) |
| Total throughput | 109.2 (1.3 IPC x 84 threads) |
| TDP | 80 W |
| L1 per thread | 4.57 KB |

### v1.3 ROB Entry Density (10 ROB density benchmarks)

| Winner | Count | Key mechanism |
|---|---|---|
| Bemi v1.3 | 10 / 10 | 84 threads x 1.3 IPC = 109.2 TP; 4B ROB entries = 3.5x density |
| Native x86 | 0 / 10 | ROB entries are 14 bytes vs Bemi's 4 bytes |

x86 loses all 10 ROB density benchmarks because Bemi v1.3 packs 3.5x more entries
into the same SRAM budget without the CAM O(n^2) scaling penalty of a monolithic ROB.

### v1.2 Model (12-benchmark suite)

| Winner | Count | Key mechanism |
|---|---|---|
| Bemi v1.2 | 9 / 12 | 144 threads x 1.3 IPC = 187.2 TP; Ring -1 trace cache |
| Native x86 | 3 / 12 | Dedicated ASIC (no passthrough); 16 KB L1/thread vs 2.67 KB |

The three x86 wins are architecturally honest. See Chapter 14 for v1.1's different loss profile.

