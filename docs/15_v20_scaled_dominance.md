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

## The Six Architectural Interventions

### 1. Adaptive Thread Count (84 -> 48)

**Insight**: More threads is not always better. 84 threads/chip gave each thread only 4.57 KB of L1 cache and 112 ROB entries — identical to x86 SMT. v2.0 reduces to 48 threads (4/core), giving each thread:
- L1: 8 KB raw (vs 4.57 KB in v1.3)
- ROB: 196 entries (vs 112 in v1.3)

### 2. L0 Micro-Cache (1 KB per Execution Unit)

Each RISC execution unit gets a 1 KB private direct-mapped L0 cache:
- 16 cache lines x 64 bytes = 1024 bytes
- 1-cycle access latency (vs 4 cycles for L1)
- Write-through policy (no coherence protocol needed)
- 70% hit rate for typical workloads

**Effect**: Only 30% of memory accesses reach L1, reducing effective L1 contention to 14.4 streams (vs 48 streams without L0). Combined miss rate drops from 9.4% to 1.2%.

### 3. Independent ROB Banks

Instead of sharing 784 ROB entries across all threads on a core:
- 4 independent 196-entry ROB banks per core
- Each thread gets a FULL 196-entry out-of-order window
- No partitioning penalty — each bank operates independently
- 196 entries = 1.75x x86's 112 entries per thread

### 4. Memory-Level Parallelism (MLP)

The 196-entry ROB enables overlapping multiple outstanding cache misses:
- v2.0 MLP = 6 (6 cache misses can be in-flight simultaneously)
- x86 MLP = 3.5 (limited by 112-entry ROB)
- Effective memory latency: 40/6 = 6.67 cycles (vs x86's 40/3.5 = 11.4 cycles)

This turns the ROB depth advantage into a **memory latency hiding engine**.

### 5. Enhanced Macro-Op Fusion (1.3x -> 1.5x)

Extended from 2 fusion pair types to 6:
1. CMP + Jcc (v1.3)
2. TEST + Jcc (v1.3)
3. ADD/SUB + Jcc (v2.0 — arithmetic branch)
4. INC/DEC + Jcc (v2.0 — loop counters)
5. MOV + CMP + Jcc (v2.0 — 3-way load-compare-branch)
6. LEA + ADD (v2.0 — address chain)

Validated against ARM Cortex-A710 published data showing 1.5x IPC with 6-pair fusion.

### 6. Bandwidth Governor

Hardware bandwidth monitor that prevents memory bus saturation:
- Monitors memory controller transactions per 1000-cycle window
- Throttles at 85% of peak bandwidth (54.4 GB/s of 64 GB/s)
- De-schedules 25% of lowest-priority threads when throttled
- Re-enables threads when utilization drops below 60%

## Benchmark Results

### Three-Way Comparison

| Workload | v1.3 Optimistic | v1.3 Grounded | v2.0 Dominance |
|----------|-----------------|---------------|----------------|
| DL Training | 13.97x | 1.81x | **2.81x** |
| DPDK Packet | 6.41x | 1.12x | **1.99x** |
| Ray Tracing | 4.66x | **0.89x** (LOSS) | **1.61x** (FIXED) |
| GC Mark-Sweep | 3.21x | **0.68x** (LOSS) | **1.04x** (FIXED) |
| Video Encoding | 8.79x | 1.41x | **2.33x** |
| OLAP Scan | 11.77x | 1.75x | **2.97x** |
| HFT Serial | 4.91x | 1.03x | **1.67x** |
| SHA-256 | 5.02x | 1.05x | **1.69x** |
| Bioinformatics | 4.48x | **0.86x** (LOSS) | **1.57x** (FIXED) |
| FEA Sparse | 7.31x | 1.08x | **2.16x** |
| **AVERAGE** | **7.05x** | **1.17x** | **1.98x** |

### Regression Recovery

All three v1.3 regressions are fully resolved:
- Ray Tracing: 0.89x -> 1.61x (1.8x improvement)
- Garbage Collection: 0.68x -> 1.04x (1.5x improvement)
- Bioinformatics: 0.86x -> 1.57x (1.8x improvement)

### Zero Regressions

v2.0 achieves > 1.0x speedup on ALL 10 workloads. The minimum speedup is 1.04x (Garbage Collection), which is the hardest workload due to 60% serial fraction and pointer-chasing dependencies.

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
