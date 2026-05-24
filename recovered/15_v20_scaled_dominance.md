# Chapter 15: Bemi v2.0 — Scaled Dominance Architecture

## Overview

Bemi v2.0 "Scaled Dominance" is the fourth major architectural iteration, designed to address the performance regressions discovered when v1.3 was subjected to physically-grounded simulation constraints.

**Problem**: v1.3 showed an average speedup of only 1.17x over x86 (down from 7.05x optimistic) and suffered outright regressions on three workloads (Ray Tracing: 0.89x, Garbage Collection: 0.68x, Bioinformatics: 0.86x).

**Root Cause**: Four physical bottlenecks — L1 cache thrashing, ROB partitioning, memory bandwidth saturation, and CISC instruction expansion — destroyed v1.3's thread-density advantage at scale.

**Solution**: v2.0 replaces the "maximum threads" philosophy with "optimal threads," implementing six architectural interventions that trade 42% thread count for 2x per-thread resource quality.

## Architecture Version History

| Version | Threads | Decode | IPC/Thread | Key Innovation |
|---------|---------|--------|------------|----------------|
| v1.0    | —       | —      | —          | Naive ROB scaling (ABANDONED) |
| v1.1    | 36      | 1-cyc  | 5.2x       | Native RISC ISA, decoder removed |
| v1.2    | 144     | 4-cyc  | 1.3x       | Weaponized x86, decoder kept for fusion |
| v1.3    | 84      | 4-cyc  | 1.3x       | ROB Entry Density (4B vs 14B) |
| **v2.0** | **48** | **4-cyc** | **1.5x** | **Scaled Dominance (L0, MLP, BW Gov)** |

## The Six Architectura
<truncated 3549 bytes>
eves > 1.0x speedup on ALL 10 workloads. The minimum speedup is 1.04x (Garbage Collection), which is the hardest workload due to 60% serial fraction and pointer-chasing dependencies.

## Physical Design Notes

### Die Area Impact

| Component | Area per Unit | Total Area |
|-----------|---------------|------------|
| L0 micro-cache | 0.05 mm^2 | 9.0 mm^2 (180 units) |
| BW governor | ~0.01 mm^2 | 0.01 mm^2 (1 unit) |
| Extended fusion logic | ~0.02 mm^2 | 0.24 mm^2 (12 clusters) |
| **Total v2.0 overhead** | — | **~9.25 mm^2** |

At 6nm with a ~100 mm^2 die, this represents ~9.25% additional area, well within the budget freed by reducing from 84 to 48 active threads.

### Power Budget

v2.0 TDP = 75W (down from v1.3's 80W):
- Fewer active threads = less dynamic power
- L0 caches reduce L1 access energy (L0 hit is ~5x cheaper than L1 hit)
- BW governor prevents memory controller power spikes

## Relationship to Previous Versions

v2.0 is a direct successor to v1.3 and retains all v1.3 innovations:
- x86 decoder KEPT for macro-op fusion (from v1.2)
- 4B compressed RISC ROB entries (from v1.3)
- Distributed/split ROB architecture (from v1.3)
- Ring -1 firmware-level DBT for legacy OS compatibility

v2.0 adds the physical-awareness layer that was missing from all previous versions.

## Source Files

| File | Description |
|------|-------------|
| `bemi_constants.py` | v2.0 architecture constants (V20_* namespace) |
| `pro-tes/performance/l0cache/L0MicroCache.c` | L0 micro-cache implementation |
| `pro-tes/performance/bwgov/BandwidthGovernor.c` | Bandwidth governor implementation |
| `pro-tes/performance/fusion/MacroOpFusion.c` | Extended 6-pair fusion engine |
| `tests/scaling_bottlenecks_test.py` | Three-way benchmark comparison |
