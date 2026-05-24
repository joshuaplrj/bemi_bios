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
| v2.0    | 48      | 4-cyc  | 1.5x              | Scaled Dominance (L0, MLP, BW Gov) |
| **v3.0**| **60**  | **1.75-cyc**| **3.66x**   | **Memory & Predictor Ascendancy (L4, HMC, PTC)** |

---

## The Four Key v3.0 Architectural Innovations

### 1. Ring -1 PTC Trace Cache

**Insight**: Keeping the legacy x86 decoder ensures compatibility and enables macro-op fusion, but caps single-thread decode throughput at 1 uop/cycle.
Bemi v3.0 adds a Ring -1 Trace Cache (PTC) that caches decoded, pre-translated, and pre-fused RISC uop loops directly.
- **PTC Hit Rate**: 75% for hot paths and loop execution
- **Bypass Benefit**: Bypasses the slow 4-cycle x86 decoder on PTC hits (which execute at 1 cycle)
- **Effective Decode Latency**: drops from 4.0 cycles to **1.75 cycles**
- **Enhanced Fusion**: 8-pair macro-op fusion (1.6x IPC multiplier)
- **Effective Peak IPC**: `(4 / 1.75) * 1.6 = 3.66` (vs 1.5 in v2.0)

### 2. 3D Stacked V-Cache (L4 Cache)

To support 60 threads without memory bus thrashing, v3.0 stacks a 128 MB SRAM die on top of the base compute die:
- **Capacity**: 128 MB
- **L4 Hit Latency**: 15 cycles (vs 40 cycles main memory latency)
- **Hit Rate**: 60% of L1/L2 cache misses are captured by L4
- **Blended Memory Latency**: reduced from 40 cycles to **25 cycles**

### 3. Hardware Memory Link Compression (HMC)

A Base-Delta-Immediate (BDI) compression unit is embedded in the memory controller's physical link layer:
- **Compression Ratio**: 1.5x on average cache line transfers
- **Effective Bandwidth**: expands dual-channel DDR5 physical limit (64 GB/s) to **96.0 GB/s**
- **BW Governor Threshold**: throttles at 85% of effective limit (**81.6 GB/s**), preventing "race to stall" bottlenecks even at 60 active threads.

### 4. Expanded ROB SRAM & MLP-8 Scaling

With 60 active threads (5/core instead of 7/core in v1.3), Bemi v3.0 doubles the ROB SRAM budget on the compute die.
- **Core ROB Budget**: 1568 entries (doubled from 784 in v1.3/v2.0)
- **ROB per Thread**: 1568 / 5 = **313 entries** (vs 196 in v2.0)
- **MLP-8**: Deep ROB enables overlapping up to 8 cache misses simultaneously
- **Effective Memory Latency**: `25 / 8.0 = 3.125 cycles` (vs 6.67 in v2.0 and 11.4 in x86)

---

## Benchmark Results

### Four-Way Comparison (Speedup vs x86 Baseline)

| Workload | v1.3 Optimistic | v1.3 Grounded | v2.0 Dominance | v3.0 Ascendancy |
|----------|-----------------|---------------|----------------|-----------------|
| DL Training | 13.97x | 1.81x | 2.81x | **4.21x** |
| DPDK Packet | 6.41x | 1.12x | 1.99x | **6.00x** |
| Ray Tracing | 4.66x | 0.89x (LOSS) | 1.61x | **4.44x** |
| GC Mark-Sweep | 3.21x | 0.68x (LOSS) | 1.04x | **2.56x** |
| Video Encoding | 8.79x | 1.41x | 2.33x | **3.49x** |
| OLAP Scan | 11.77x | 1.75x | 2.97x | **8.02x** |
| HFT Serial | 4.91x | 1.03x | 1.67x | **4.14x** |
| SHA-256 | 5.02x | 1.05x | 1.69x | **4.23x** |
| Bioinformatics | 4.48x | 0.86x (LOSS) | 1.57x | **4.30x** |
| FEA Sparse | 7.31x | 1.08x | 2.16x | **6.88x** |
| **AVERAGE** | **7.05x** | **1.17x** | **1.98x** | **4.83x** |

### Zero Regressions

Like v2.0, Bemi v3.0 achieves zero regressions, but with massive performance scaling. The minimum speedup is now **2.56x** (Garbage Collection), while peak speedup reaches **8.02x** (OLAP Scan) and **6.00x** (DPDK Packet Processing).

---

## Physical Design Notes

### Die Area & Packaging Overhead

Stacking a 128 MB L4 cache requires a 3D-stacked IC packaging process (e.g., TSMC SoIC).

| Component | Area per Unit | Total Area |
|-----------|---------------|------------|
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
