# Chapter 16: Bemi v3.0 — Memory & Predictor Ascendancy Architecture

## Overview

Bemi v3.0 "Memory & Predictor Ascendancy" is the fifth major architectural iteration, designed to eliminate the remaining decode and memory bandwidth bottlenecks in v2.0 while scaling execution threads.

**Problem**: While v2.0 successfully resolved all v1.3 regressions (achieving an average speedup of 1.98x), scaling beyond 48 threads was bottlenecked by the 64 GB/s physical memory bandwidth limit of DDR5 and the 4-cycle decode latency of the legacy x86 decoder.

**Solution**: v3.0 scales active threads to 60 (12 cores x 5 threads/core) and introduces three major hardware enhancements: 3D Stacked V-Cache (L4), Hardware Link Compression (HMC), and a Ring -1 Trace Cache (PTC). It also doubles the ROB SRAM budget per core, providing 313 entries per thread. This yields an average speedup of **4.83x** over the x86 baseline with zero regressions.

---

## Architecture Version History

| Version | Threads | Decode | IPC/Thread (peak) | Key Innovation |
|---------|---------|--------|-------------------|----------------|
| v1.0    | —       | —      | —                 | Naive ROB scaling (ABANDONED) |
| v1.1    | 36      | 1-cyc  | 5.2x              | Native RISC ISA, decoder removed |
| v1.2    | 144     | 4-cyc  | 1.3x              | Weaponized x86, decoder kept for fusion |
| v1.3    | 84      | 4-cyc  | 1.3x              | ROB Entry Density (4B vs 14B) |
| v2.0    | 48      | 4-cyc  | 1
<truncated 3789 bytes>
---------------|------------|
| L0 micro-cache | 0.05 mm² | 12.0 mm² (240 units) |
| BW governor | ~0.01 mm² | 0.01 mm² (1 unit) |
| PTC Trace Cache SRAM | ~0.25 mm² | 3.00 mm² (12 cores) |
| Doubled ROB SRAM | ~0.50 mm² | 6.00 mm² (12 cores) |
| **Silicon Area Overhead (Compute Die)** | — | **~21.01 mm²** |
| **3D Stacked L4 Cache SRAM Die** | — | **~36.00 mm²** |

The base compute die overhead is ~21.0 mm², which is viable on a ~120 mm² chip at 6nm. Stacking the 36.0 mm² cache die vertically does not increase the horizontal motherboard area footprint.

### Power Budget & TDP

- **TDP target**: **85W** (equal to Bemi v1.2, up from v2.0's 75W)
- Stacking the 128 MB L4 cache adds ~10W of static and dynamic power overhead.
- Because v3.0 executes tasks much faster, the **energy-to-completion** ($E = \text{TDP} \times \text{Time}$) is cut in half compared to v2.0, making v3.0 the most energy-efficient Bemi architecture.

---

## Source Files

| File | Description |
|------|-------------|
| [bemi_constants.py](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/bemi_constants.py) | v3.0 architecture constants (`V30_*` namespace) |
| [TraceCache.c](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/bemi_bios/pro-tes/performance/tcache/TraceCache.c) | pre-translation trace cache UEFI simulator |
| [MemoryCompressor.c](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/bemi_bios/pro-tes/performance/bwgov/MemoryCompressor.c) | HMC link compression UEFI simulator |
| [scaling_bottlenecks_test.py](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/tests/scaling_bottlenecks_test.py) | Four-way evaluation test runner |
| [run_all_benchmarks.py](file:///c:/Users/John%20Jacob/Desktop/extras/test-box/vemi/bemi_bios/run_all_benchmarks.py) | Full suite benchmark executor |
