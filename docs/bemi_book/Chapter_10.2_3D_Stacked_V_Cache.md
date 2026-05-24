# Chapter 10: Bemi v3.0: Memory & Predictor Ascendancy Architecture

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
