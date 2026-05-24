# Chapter 10: Bemi v3.0: Memory & Predictor Ascendancy Architecture

## 10.1 Ring -1 PTC Trace Cache

### 10.1.1 Bypassing the Front-End Decode Cap
In SMT architectures utilizing standard CISC decoders, instructions must be parsed sequentially to identify variable lengths and instruction boundaries. Even with the macro-op fusion improvements introduced in Bemi v2.0, the physical x86 decoder remaining on the silicon die imposes a hard constraint: it can decode at most 4 simple or 1 complex x86 instruction per cycle. This translates to an effective throughput limit of 1 uop/cycle per SMT thread.

To break this front-end ceiling, Bemi v3.0 introduces a dedicated **Pre-translation Trace Cache (PTC)** operating at the Ring -1 firmware boundary.

```
                  +----------------------------------+
                  |           Instruction            |
                  +----------------------------------+
                                   |
                                   v
                      +--------------------------+
                      |    PTC Tag Matching      |
                      +--------------------------+
                       /                        \
           (Hit: 75%) /                          \ (Miss: 25%)
                     v                            v
       +---------------------------+        +---------------------------+
       |   PTC Trace Buffer        |        |   Legacy x86 Decoder      |
       |   (Direct Execution)      |        |   (4-Cycle Latency)       |
       +---------------------------+        +---------------------------+
                     \                            /
                      \                          /
                       v                        v
                  +----------------------------------+
                  |      Execution Engine Pipeline   |
                  +----------------------------------+
```

### 10.1.2 PTC Structure and Latency Equations
The PTC caches pre-decoded, pre-translated, and pre-fused RISC micro-operations directly.
- **Hit Rate:** Verified at **75%** for loop-heavy workloads and standard execution hot paths.
- **Latency Profile:** PTC hits execute with a $1\text{-cycle}$ latency, fully bypassing the legacy $4\text{-cycle}$ x86 decoder.
- **Effective Decode Latency Formula:**
  $$ \text{Latency}_{\text{effective}} = (\text{HitRate}_{\text{PTC}} \times 1) + ((1 - \text{HitRate}_{\text{PTC}}) \times \text{Latency}_{\text{decoder}}) $$
  $$ \text{Latency}_{\text{effective}} = (0.75 \times 1) + (0.25 \times 4) = 0.75 + 1.0 = 1.75 \text{ cycles} $$

### 10.1.3 Expanding the Fusion Window
By bypassing the sequential decoder pipeline on PTC hits, Bemi v3.0 expands its macro-op fusion engine to support **8-pair group-fused execution patterns** (raising the IPC multiplier to **1.60**). 
The effective peak IPC per thread scales significantly:
$$ \text{IPC}_{\text{peak}} = \frac{\text{Width}_{\text{dispatch}}}{\text{Latency}_{\text{effective}}} \times \text{Multiplier}_{\text{fusion}} $$
$$ \text{IPC}_{\text{peak}} = \frac{4}{1.75} \times 1.60 = 3.66 $$
This represents a $2.44\times$ increase over Bemi v2.0's peak IPC ($1.5$).

---

## 10.2 3D Stacked V-Cache (L4 Cache)

### 10.2.1 The Interconnect Latency Wall
As virtual thread counts scale to **60 threads** (5 threads/core across 12 cores), competing demands for cache lines cause frequent evictions in the L1 and L2 caches. When an L2 miss occurs, the CPU must navigate the on-die Ring Bus or Mesh Interconnect to fetch from the shared L3 cache. If the L3 cache also misses, the transaction must traverse the memory controller to DRAM, incurring a painful 40-cycle penalty.

To capture these misses before they reach the high-latency DRAM interface, Bemi v3.0 utilizes TSV (Through-Silicon Via) technology to stack an auxiliary SRAM die directly on top of the base compute die.

### 10.2.2 3D Stacked L4 Cache Specifications
- **Capacity:** $128 \text{ MB}$ of high-density SRAM.
- **Latency:** $15 \text{ cycles}$ hit latency.
- **Capture Rate:** Captures **60%** of all combined L1/L2 cache misses.
- **Blended Memory Latency Calculation:**
  $$ \text{Latency}_{\text{blended}} = (\text{HitRate}_{L4} \times \text{Latency}_{L4}) + ((1 - \text{HitRate}_{L4}) \times \text{Latency}_{\text{DRAM}}) $$
  $$ \text{Latency}_{\text{blended}} = (0.60 \times 15) + (0.40 \times 40) = 9.0 + 16.0 = 25.0 \text{ cycles} $$
Stacking the 128 MB V-Cache die cuts the blended memory penalty from 40 cycles down to **25 cycles**, shielding the execution units from raw DRAM roundtrip latencies.

---

## 10.3 Hardware Memory Link Compression (HMC)

### 10.3.1 Overcoming the Pin-Count Bandwidth Barrier
Physical memory bandwidth is limited by the number of pins on the processor package and the frequency of the DDR5 bus. The dual-channel DDR5 configuration on our baseline motherboard is capped at a physical limit of **64 GB/s**. 

Rather than waiting for wider physical buses, Bemi v3.0 implements **Hardware Memory Link Compression (HMC)** directly within the physical link layer of the memory controller.

### 10.3.2 Base-Delta-Immediate (BDI) Link Compression
The HMC unit utilizes a low-latency **Base-Delta-Immediate (BDI)** hardware compression algorithm. Since adjacent data values in memory cache lines (such as arrays of integers, pointers, or structures) often differ by small delta offsets, BDI represents the cache line using a common base value and an array of small deltas:

```
Uncompressed Cache Line:  [ 0x0000000000102000 | 0x0000000000102008 | 0x0000000000102010 ]
Compressed (BDI):          Base: 0x0000000000102000, Deltas: [ 0, +8, +16 ]  (Saves 60% Space)
```

- **Compression Ratio:** Achieves an average **1.5x compression** on standard cache line transfers.
- **Effective Memory Bandwidth:**
  $$ \text{Bandwidth}_{\text{effective}} = \text{Bandwidth}_{\text{physical}} \times \text{Ratio}_{\text{compression}} $$
  $$ \text{Bandwidth}_{\text{effective}} = 64 \text{ GB/s} \times 1.5 = 96.0 \text{ GB/s} $$
- **Bandwidth Governor Adjustments:** The Bandwidth Governor (introduced in Section 9.6) is updated to throttle at 85% of this new effective limit: **81.6 GB/s**, preventing memory queues from stalling under 60-thread workloads.

---

## 10.4 Expanded ROB and MLP-8 Scaling

### 10.4.1 Allocating the ROB Budget for SMT-5
With 60 active threads (5 SMT threads per core), Bemi v3.0 doubles the physical ROB SRAM budget on the compute die from 784 entries to **1568 entries per core**.
- **Allocation:** Each of the 5 SMT threads is mapped to a dedicated, physically isolated bank of **313 ROB entries** (up from 196 entries in v2.0).
- **Associative Search Window:** This 313-entry out-of-order window represents a $2.79\times$ increase over native x86's SMT ROB portion (112 entries).

### 10.4.2 Achieving Memory-Level Parallelism (MLP) of 8
The expanded 313-entry ROB bank enables the out-of-order engine to look much further ahead in the instruction stream to identify independent memory instructions. This increases the average **MLP to 8** (supporting up to 8 concurrent cache misses).

**Effective Latency calculation under v3.0 parameters:**
$$ \text{Latency}_{\text{effective}} = \frac{\text{Latency}_{\text{blended}}}{\text{MLP}} $$
$$ \text{Latency}_{\text{effective}} = \frac{25 \text{ cycles}}{8.0} = 3.125 \text{ cycles} $$

By combining Stacked V-Cache and HMC compression (reducing blended latency to 25 cycles) with deep MLP-8 OoO scheduling, Bemi v3.0 achieves an effective memory latency of **3.125 cycles** (a 72% reduction compared to native x86's 11.43 cycles).

---

## 10.5 Version 3.0 Performance Progression

Bemi v3.0's combined memory and branch prediction enhancements deliver exceptional performance scaling:

```
  Workload            x86 Base   Bemi v1.3    Bemi v2.0    Bemi v3.0   Net Speedup (v1.3->v3.0)
  ------------------ ---------- ----------- ------------ ------------ --------------------------
  DL Training          1.00x       1.81x       2.81x      **4.21x**             2.32x
  DPDK Packet          1.00x       1.12x       1.99x      **6.00x**             5.35x
  Ray Tracing          1.00x       0.89x       1.61x      **4.44x**             4.98x
  Garbage Collection   1.00x       0.68x       1.04x      **2.56x**             3.76x
  Video Encoding       1.00x       1.41x       2.33x      **3.49x**             2.47x
  OLAP Scan            1.00x       1.75x       2.97x      **8.02x**             4.58x
  HFT Serial           1.00x       1.03x       1.67x      **4.14x**             4.01x
  SHA-256 Hashing      1.00x       1.05x       1.69x      **4.23x**             4.02x
  Bioinformatics       1.00x       0.86x       1.57x      **4.30x**             5.00x
  FEA Sparse Solver    1.00x       1.08x       2.16x      **6.88x**             6.37x
  ------------------ ---------- ----------- ------------ ------------ --------------------------
  AVERAGE              1.00x       1.17x       1.98x      **4.83x**             4.13x
```

Through Stacked V-Cache, HMC link compression, and the PTC trace cache, Bemi v3.0 achieves a massive **4.83x average speedup** over the x86 baseline, demonstrating the power of hardware-firmware co-design in conquering the memory wall.
