# Chapter 17: Bemi v4.0 — Ultra-Bandwidth & Execution Zenith Architecture

## Overview

Bemi v4.0 "Ultra-Bandwidth & Execution Zenith" is the sixth major architectural iteration, designed to maximize multi-thread scaling while completely eliminating execution latency and memory link bottlenecks.

**Problem**: Even with Bemi v3.0's 1.5x Link Compression (HMC) and 128 MB V-Cache, heavy memory-bound workloads (DL Training, Video Encoding, OLAP Scan) suffered from severe bandwidth governor throttling, losing up to 60% of their performance potential. Additionally, serial execution phases thrashed resources across small 313-entry SMT ROB windows.

**Solution**: v4.0 scales active threads to 72 (12 cores x 6 threads/core) and introduces four major microarchitectural innovations:
1. **Adaptive HMC**: Pattern-based (FPC/FDC) compression yielding up to 2.2x ratio, extending physical bandwidth to **140.8 GB/s**.
2. **256 MB Stacked V-Cache v2.0**: Captures **80%** of L1/L2 misses, reducing blended memory latency to **20 cycles**.
3. **Neural Perceptron branch predictor**: Achieves **88% hit rate** in the PTC trace cache, dropping effective decode latency to **1.35 cycles** and enabling **10-pair macro-op fusion**.
4. **Dynamic Core/Thread Fusion (DCF)**: Fuses thread resources during serial execution phases to yield a single "Super-Thread" with a **626-entry ROB** and **MLP-12**.

This yields an average speedup of **6.75x** over the x86 baseline with zero regressions.

---

## Architecture Version History

| Version | Threads | Decode | IPC/Thread (peak) | Key Innovation |
|---------|---------|--------|-------------------|----------------|
| v1.0    | —       | —      | —                 | Naive ROB scaling (ABANDONED) |
| v1.1    | 36      | 1-cyc  | 5.2x              | Native RISC ISA, decoder removed |
| v1.2    | 144     | 4-cyc  | 1.3x              | Weaponized x86, decoder kept for fusion |
| v1.3    | 84      | 4-cyc  | 1.3x              | ROB Entry Density (4B vs 14B) |
| v2.0    | 48      | 4-cyc  | 1.5x              | Scaled Dominance (L0, MLP, BW Gov) |
| v3.0    | 60      | 1.75-cyc| 3.66x            | Memory & Predictor Ascendancy (L4, HMC, PTC) |
| **v4.0**| **72**  | **1.35-cyc**| **5.18x**    | **Ultra-Bandwidth & Execution Zenith (Adaptive HMC, DCF, NPP)** |

---

## The Four Key v4.0 Architectural Innovations

### 1. Neural Perceptron Predictor (NPP) & 10-Pair Fusion

By replacing TAGE branch prediction with neural perceptron arrays, the branch prediction accuracy is boosted significantly.
- **PTC Hit Rate**: rises from 75% to **88%** on loops and hot blocks.
- **Effective Decode Latency**: drops from 1.75 cycles to **1.35 cycles** on blended hit/miss execution paths.
- **10-Pair Fusion**: Supports advanced group-fused execution patterns (e.g. nested jumps, 3-instruction ALU chains), providing a **1.75x** IPC multiplier.
- **Effective Peak IPC**: `(4 / 1.35) * 1.75 = 5.18` (vs 3.66 in v3.0).

### 2. 256 MB Stacked V-Cache v2.0

We double the L4 cache die capacity to 256 MB and implement a 512-bit internal L4-to-L3 bus using 3D vertical IC packaging (TSMC SoIC):
- **Miss Capture Rate**: **80%** of L1/L2 misses are filtered by L4.
- **Blended Memory Latency**: reduced from 25 cycles to **20 cycles**.

### 3. Adaptive Hardware Memory Compression (Adaptive HMC)

Rather than using a fixed Base-Delta-Immediate (BDI) compression ratio, the memory controller dynamically detects data patterns and selects the most efficient compression algorithm:
- **DL Training**: Dynamic 2.2x compression (effective peak BW: **140.8 GB/s**).
- **OLAP Scan**: 2.0x column compression (effective peak BW: **128.0 GB/s**).
- **Video Encoding**: 1.8x pixel stream compression (effective peak BW: **115.2 GB/s**).
- **Other Workloads**: 1.5x fallback compression (effective peak BW: **96.0 GB/s**).

This eliminates the bandwidth governor bottleneck for memory-heavy workloads, lifting execution throttles completely.

### 4. Dynamic Core/Thread Fusion (DCF)

During parallel execution phases, cores operate in standard SMT-6 mode (each virtual thread gets a 313-entry ROB and MLP-10). When a core detects serial bottlenecks or synchronization gates:
- It fuses adjacent virtual thread execution ports and ROB banks.
- The thread runs in "Fused Super-Thread" mode with **626 ROB entries**.
- **MLP-12**: Overlaps up to 12 cache misses simultaneously.
- **Effective Memory Latency**: `20 / 12 = 1.67 cycles` (compared to 3.125 in v3.0).

---

## Physical Design Notes

### Die Area & Packaging Overhead

Doubling stacked cache size and adding perceptron arrays increases silicon footprint:

| Component | Area per Unit | Total Area |
|-----------|---------------|------------|
| L0 micro-cache | 0.05 mm² | 12.0 mm² (240 units) |
| BW governor | ~0.01 mm² | 0.01 mm² (1 unit) |
| PTC Trace Cache SRAM | ~0.25 mm² | 3.00 mm² (12 cores) |
| Doubled ROB SRAM & DCF | ~0.80 mm² | 9.60 mm² (12 cores) |
| Neural Perceptron Predictor | ~0.03 mm² | 0.36 mm² (12 cores) |
| **Silicon Area Overhead (Compute Die)** | — | **~24.97 mm²** |
| **3D Stacked L4 Cache SRAM Die (256MB)**| — | **~64.00 mm²** |

The ~25.0 mm² compute die overhead remains very manageable on a ~120 mm² 6nm die. The stacked 64 mm² V-Cache die lies vertically above the compute units, causing no increase in motherboard horizontal layout.

### Power Budget & TDP

- **TDP target**: **90W** (up from v3.0's 85W).
- The 256 MB stacked L4 cache and perceptron logic add ~5W.
- **Energy Efficiency**: Due to the 6.75x average execution speedup, the total energy consumed to complete a workload ($E = \text{TDP} \times \text{Time}$) is reduced by 35% compared to Bemi v3.0.
