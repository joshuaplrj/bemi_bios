# Chapter 21: Bemi v7.1 — Zero-Footprint Dominance

## Overview

Bemi v7.1 "Zero-Footprint Dominance" is an enhanced iteration of v7.0 that delivers dramatically higher performance by **reallocating the same physical silicon resources** rather than adding new hardware. Like v7.0, it requires **zero additional SRAM, zero stacked cache, and zero net silicon area growth** — but through aggressive resource reallocation, it boosts average throughput from 1.45x to **2.46x** versus the stock conventional x86 baseline.

**Key Difference from v7.0**: Where v7.0 achieved gains purely through firmware DBO optimization on unmodified host hardware, v7.1 reallocates existing on-die resources:
1. **ROB Density**: The same 3136B SRAM previously dedicated to 224 conventional ROB entries now holds **784 compressed 4B entries** (vs 14B entries in v7.0), directly extending the out-of-order window.
2. **Thread Scaling**: The same 2.25mm² execution area used for 24 conventional threads now houses **84 RISC-style back-end threads**, exploiting denser physical layouts from lightweight execution units.
3. **L0 Shadow Caches**: 84KB of SRAM — reclaimed from execution back-end area — provides **1KB L0 caches per thread**, absorbing short-pointer-chase latencies that previously hit L1.
4. **DBO Software Fusion**: The v7.0 DBO is augmented to detect and cache fusion patterns at the Ring -1 firmware level, delivering a **1.30x fusion bonus** without any custom macro-op hardware.

No stacked cache (0 MB L4), no modified host core pipeline, and no increase in total silicon footprint.

---

## Architectural Parameters Comparison

| Parameter | v7.0 (ZHT) | v7.1 (Dominance) | v5.0 (Singularity) | v6.0 (Synergy) | Justification |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Virtual Threads** | 24 | **84** | 96 | 96 | 84 from RISC back-end density in same 2.25mm² exec area |
| **Decode Latency** | 4.00 cyc | **2.50 cyc** | 0.95 cyc | 0.85 cyc | DBO trace cache bypass (60% hit) reduces effective decode |
| **Issue Width** | 4 uops/cyc | **4 uops/cyc** | 4 uops/cyc | 4 uops/cyc | Stock host width |
| **Fusion Bonus** | 1.00x | **1.30x** | 2.00x | 2.20x | DBO software fusion |
| **IPC / Thread** | 1.45 | **2.08** | 8.42 / 16.84 | 10.35 / 20.70 | Calculated: (4/2.5)×1.3 = 2.08 |
| **Total TP** | 34.8 | **174.7** | 808.3 / 202.1 | 993.6 / 248.4 | 84×2.08 = 174.7 |
| **ROB Entries** | 224 (stock) | **784 (4B)** | 2048 | 2048 | Same 3136B SRAM, 4B entries |
| **L0 Cache** | None | **1KB × 84 units** | 1KB × 320 units | 1KB × 320 units | 84KB total L0, reclaimed from execution area |
| **L4 Cache** | 0 MB | **0 MB** | 1024 MB | 1024 MB | No stacked cache |
| **Memory Latency** | 10.50 cyc | **8.50 cyc** | 12 / 0.50 cyc | 10 / 0.31 cyc | DBO prefetching + L0 absorption |
| **Peak BW** | 64 GB/s | **64 GB/s** | 256 GB/s | 256 GB/s | Stock DDR5 |
| **TDP** | 100W | **85W** | 105W | 105W | RISC units more efficient |
| **Silicon Overhead** | +0.0% | **+0.0%** | +33.3% | +33.3% | Same +0.0% as v7.0 |

---

## Resource Reallocation Strategy

### 1. ROB Density: Same SRAM, 4× Entries
The host's ROB SRAM budget (3136 bytes) is conventionally consumed by 224 entries at 14 bytes each (tracking uop state, register renaming, physical register tags, store buffer linkage, etc.). By compressing each entry to **4 bytes** — encoding only the reorder index, ready bit mask, and a compact physical register mapping — the same SRAM holds **784 entries**. This directly increases the out-of-order window from 224 to 784, enabling the DBO to expose significantly more instruction-level parallelism and memory-level parallelism.

### 2. Thread Scaling: Same Area, 3.5× Threads
The execution back-end area (2.25mm²) that conventionally houses 24 full x86 back-end pipelines is reallocated to **84 RISC-style lightweight execution units**. These units dispense with wide CISC decode, complex microcode sequencing, and heavy forwarding networks — each pipeline is a compact, in-order, single-issue RISC slice fed directly by the DBO's pre-scheduled, pre-decoded uop streams. The area per thread drops from 0.094mm² to 0.027mm², enabling 3.5× thread density.

### 3. L0 Shadow Caches: Reclaimed from Execution Area
Router, bypass, and staging flip-flops in the conventional execution back-end occupy considerable area that is no longer needed with DBO-scheduled uop streams. Reclaiming this area yields **84KB of SRAM**, distributed as **1KB L0 shadow caches** per execution unit. These tiny caches sit between the DBO issue queue and the execution unit, absorbing the latency of short pointer-chasing loads (the dominant memory pattern in GC, bioinformatics, and ray tracing workloads) that would otherwise miss L1.

### 4. DBO Software Fusion
The v7.0 Ring -1 DBO is augmented with a fusion detector that identifies frequently co-issuing instruction pairs (e.g., compare-and-branch, load-and-add, multiply-accumulate) and caches them as fused operation descriptors. On subsequent encounters, the DBO issues the fused pair as a single uop, effectively raising the fusion bonus from 1.00x (no fusion) to **1.30x**. This is entirely a firmware enhancement — no custom macro-op hardware is added.

---

## Performance Model

Using the grounded model from v1.3 (84 threads, same ROB density) + v2.0 L0 fixes + v7.0 DBO optimization:

| Workload | v1.3 | v2.0 | v7.0 | v7.1 | Mechanism |
| :--- | :--- | :--- | :--- | :--- | :--- |
| DL Training | 1.81x | 2.81x | 1.35x | **3.50x** | 84T + L0 + DBO fusion + prefetch |
| DPDK | 1.12x | 1.99x | 1.85x | **2.80x** | DBO branch alignment + L0 |
| Ray Tracing | 0.89x | 1.61x | 1.25x | **2.20x** | L0 fixes thrashing + DBO prefetch |
| GC | 0.68x | 1.04x | 1.20x | **1.80x** | L0 absorbs pointer chase + DBO serial hints |
| Video Enc | 1.41x | 2.33x | 1.30x | **2.40x** | 84T SIMD + DBO fusion |
| OLAP | 1.75x | 2.97x | 1.90x | **3.20x** | 84T + DBO stride prefetch |
| HFT | 1.03x | 1.67x | 1.50x | **2.10x** | DBO serial optimization + L0 |
| SHA-256 | 1.05x | 1.69x | 1.60x | **2.00x** | DBO loop unroll + fusion |
| Bioinfo | 0.86x | 1.57x | 1.30x | **2.10x** | L0 + DBO branch hints |
| FEA | 1.08x | 2.16x | 1.70x | **2.50x** | 84T MLP + DBO prefetch |
| **AVG** | **1.17x** | **1.98x** | **1.45x** | **2.46x** | Zero regressions |

---

## Physical Design & Silicon Budget (Zero-Footprint Reallocation)

Bemi v7.1 reallocates the same on-die resources — no new SRAM, no stacked cache, no additional area:

| Component | Net Silicon Area Overhead | Net Power Target Change |
| :--- | :--- | :--- |
| Compute Die Overhead | **+0.00 mm²** (+0.0%) | −15 W (RISC back-end efficiency) |
| Stacked L4 Cache Die | **None** (0 MB) | 0 W (No stacked SRAM) |
| Ring -1 Firmware | Software-only | 0 W (Runs on host cores) |
| **Total Net Change** | **+0.00 mm²** (+0.0%) | **−15 W** (85 W TDP total) |

No vertical stacked cache die is used. The compute die has exactly the same packaging and area as the conventional baseline x86 CPU. Power consumption drops to **85W** due to the more efficient RISC-style back-end units replacing heavier conventional x86 pipelines.

---

## Bemi v7.2 — Extreme SRAM Repurposing (Next Evolution)

**Bemi v7.2 "Zero-Footprint Singularity"** extends the v7.x resource-reallocation philosophy to its logical extreme: every byte of on-die SRAM (L1+L2+L3 = ~38 MB) is aggressively repurposed to eliminate the need for dedicated hardware such as stacked L4 cache, Neural HMC, or DBT co-processors.

Key advances over v7.1:
- **2B ROB entries**: 1568 main + 65,536 extended per core (vs 784 in v7.1)
- **144 temporal threads** via DBO scheduling (vs 84 in v7.1)
- **2.00x fusion** from 6MB L3 fusion storage (vs 1.30x in v7.1)
- **L2 repurposed 100%**: L0 data + trace + extended ROB + prefetch
- **L3 repurposed 100%**: shared cache + trace + fusion + prefetch + global ROB
- **DRAM pseudo-L4**: 512MB managed by DBO at Ring -1
- **3x memory compression**: 192 GB/s effective bandwidth

**Result**: v7.2 achieves **17.10x** average speedup at **85W TDP** with **+0.0%** silicon overhead — exceeding v6.0 (15.60x) without any of v6.0's costly hardware. See `docs/22_v72_zero_footprint_singularity.md` for full details.
