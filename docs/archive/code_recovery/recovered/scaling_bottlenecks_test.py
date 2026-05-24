import math
import pandas as pd
import numpy as np

# -----------------------------------------------------------------------------
# ARCHITECTURE CONSTANTS (Baseline vs Bemi v1.3)
# -----------------------------------------------------------------------------
PHYSICAL_CORES = 12
CLOCK_FREQ_GHZ = 3.5

# x86 Baseline
X86_THREADS = 24
X86_ROB_DEPTH = 224
X86_L1_KB = 32
X86_TDP = 100.0

# Bemi v1.3
BEMI_THREADS = 84
BEMI_ROB_DEPTH = 784
BEMI_L1_KB = 32
BEMI_TDP = 80.0

# General Microarchitectural Assumptions
MEM_INST_FRACTION = 0.25      # 25% of instructions are memory loads/stores
L1_HIT_LATENCY = 4            # cycles
MEM_MISS_LATENCY = 40         # cycles (L1 miss -> L2/L3/RAM average)
PEAK_MEM_BANDWIDTH_GBS = 64.0 # peak dual-channel DDR5 bandwidth in GB/s

# Workload Definitions (Same as rob_dbt_benchmarks.py)
WORKLOADS = {
    "DL Training": {
        "ipc_max": 3.2, "k_sat": 256, "cycles_per_op": 8, "serial_pct": 0.01, "risc_expansion": 1.3
    },
    "DPDK Packet Processing": {
        "ipc_max": 1.8, "k_sat": 128, "cycles_per_op": 12, "serial_pct": 0.05, "risc_expansion": 1.3
    },
    "Ray Tracing": {
        "ipc_max": 1.4, "k_sat": 96, "cycles_per_op": 14, "serial_pct": 0.15, "risc_expansion": 1.3
    },
    "Garbage Collection": {
        "ipc_max": 0.6, "k_sat": 32, "cycles_per_op": 20, "serial_pct": 0.60, "risc_expansion": 1.5
    },
   
<truncated 9293 bytes>
.to_string(index=False, formatters={
        "Opt Bemi TP": "{:,.2f}".format,
        "Gr Bemi TP": "{:,.2f}".format,
        "Bemi Mem Req GB/s": "{:,.1f}".format,
        "x86 Mem Req GB/s": "{:,.1f}".format,
    }))

    print("\n" + "=" * 80)
    print("                                 KEY FINDINGS")
    print("=" * 80)
    
    avg_opt_speedup = df["Opt Speedup"].mean()
    avg_gr_speedup = df["Gr Speedup"].mean()
    print(f"Average Speedup across all workloads:")
    print(f"  - Optimistic (Original Model)   : {avg_opt_speedup:.2f}x Bemi superiority")
    print(f"  - Grounded (Physical Constraints): {avg_gr_speedup:.2f}x Bemi superiority")
    print()
    print("Critical Architectural Insights:")
    print("1. Thread Count & ROB Size Double-Scaling Correction:")
    print("   - Removing the quadratic scaling bug in Amdahl's Law reduces the theoretical Bemi speedup.")
    print("2. Cache Contention Penalty:")
    print("   - Splitting 32 KB L1 cache among 84 threads increases Bemi's cache miss rate from 5.0% to 9.4%.")
    print("   - Because Bemi's virtual threads must share the 784-entry ROB, the effective window per thread")
    print("     is only 112 entries (identical to x86 SMT's 112 entries per thread), removing Bemi's IPC advantage")
    print("     from a deeper out-of-order execution window. Bemi stalls directly on cache misses.")
    print("3. Memory Bandwidth Saturation:")
    print("   - For compute-heavy workloads (DL Training, Video, OLAP), running 84 threads at 1.3 IPC requested")
    print("     between 80 GB/s and 120 GB/s of bandwidth, far exceeding the 64 GB/s physical memory limit.")
    print("     This saturates the memory bus, dragging Bemi's speedup down to near-parity with x86.")
    print("=" * 80)

if __name__ == "__main__":
    run()
