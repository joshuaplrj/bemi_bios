# 05. Benchmarks and Peak Efficiency

## The Final Comprehensive Benchmark
To validate the final Native Bemi Architecture, a comprehensive python simulation (`final_benchmarks.py`) was developed. This suite tests both **Raw Power** (Execution Ticks) and **Peak Efficiency** (Total Energy Consumed in Joules).

### Architectural Parameters
The benchmark contrasts two physical processor models:
1. **Native x86 Model:** 12 heavy CISC cores bounded by variable-length decoders. **TDP: 100 Watts**.
2. **Native Bemi Model:** 36 dense, lean RISC cores processing static 4-byte micro-ops. **TDP: 45 Watts** (achieved by stripping out the massive x86 decoder silicon).

### The Workloads
1. **General Integer Math:** Tests pure 0-cycle decode advantages.
2. **AVX-512 (Vector Math):** Tests Macro-Op Hardware Passthrough on vector ALUs.
3. **AES-NI (Cryptography):** Tests Macro-Op Hardware Passthrough on ASIC crypto silicon.
4. **String Copy (`REP MOVSB`):** Tests Macro-Op Hardware Passthrough on microcode memory sequencers.

## Benchmark Results

### Raw Power (Execution Time)
Calculated via Total Cycles ÷ Thread Density.
* Bemi achieves a **15.0x Speedup** in general math by bypassing the 4-cycle variable decode stall entirely.
* Bemi achieves a **6.0x to 9.0x Speedup** in hardware-accelerated workloads via Macro-Op passthroughs.

### Peak Efficiency (Energy Consumption)
Calculated via TDP (Watts) $\times$ Execution Time (Ticks).
* Because Bemi finishes the workload up to 15x faster while drawing less than half the power (45W vs 100W), the energy savings are compounding.
* Bemi consumes **13.3x to 33.3x less total energy (Joules)** than native x86 to compute the exact same mathematical output.

These models confirm that the Native Bemi Architecture achieves generational leaps in both computational throughput and thermal efficiency, successfully rendering variable-length CISC models obsolete.
