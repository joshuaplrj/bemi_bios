"""
Bemi v1.3 ROB Entry Density -- 10 Workload Benchmarks
======================================================
Compares x86 (14B ROB entries, monolithic CAM) vs Bemi v1.3 (4B ROB entries,
split/distributed, no CAM bottleneck) across 10 benchmark workloads.

Each workload models its unique instruction mix, ILP characteristics,
memory access pattern, and ROB depth sensitivity.

Constants from bemi_constants.py. Uses v1.3 ROB Entry Density model.
"""

import sys
import os
import math
import pandas as pd

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import bemi_constants as C


# =============================================================================
# ROB IPC Model
# =============================================================================

class ROBModel:
    """
    IPC(depth) = IPC_max * (1 - exp(-depth / K_sat))
    Monolithic x86: CAM penalty = 1 + log2(depth/100) * 0.3 per doubling
    Split RISC: no CAM penalty
    """

    def __init__(self, ipc_max, sat_constant, cam_per_dbl, base_depth, monolithic):
        self.ipc_max = ipc_max
        self.k_sat = sat_constant
        self.cam_p = cam_per_dbl
        self.base = base_depth
        self.mono = monolithic

    def ipc_raw(self, depth):
        if depth <= 0:
            return 0.0
        return self.ipc_max * (1.0 - math.exp(-depth / self.k_sat))

    def cam_factor(self, dept
<truncated 12558 bytes>
h benefit to 5x",
        "Bioinformatics (Smith-Waterman)": "Diagonal deps + branching benefit moderately",
        "FEA (Sparse Matrix)":           "FP latency tolerance from deeper window: 7.3x",
    }
    for row in all_rows:
        sp = row["Speedup (v1.3/x86)"]
        verdict = "v1.3"
        reason = reasons.get(row["Workload"], "ROB density + thread advantage")
        print(f"  {row['Workload']:<35} {sp:<10.2f} {verdict:<10} {reason:<42}")

    print(f"\n  Final Score: v1.3 {v13_wins} / x86 {x86_wins} / Tie {ties}")

    # --- Cross-validation ---
    print()
    print(SECTION)
    print("  CROSS-VALIDATION WITH bemi_constants.py")
    print(SECTION)
    print(f"\n  Bemi v1.3 constant check:")
    print(f"    V13_THREADS         = {C.V13_THREADS}  (24 baseline x 3.5 ROB density)")
    print(f"    V13_DECODE          = {C.V13_DECODE}  (x86 decoder kept for fusion)")
    print(f"    V13_IPC             = {C.V13_IPC}  (4/4 x 1.3 fusion)")
    print(f"    V13_TOTAL_TP        = {C.V13_TOTAL_TP}  (1.3 x {C.V13_THREADS})")
    print(f"    V13_TDP             = {C.V13_TDP}W")
    print(f"    V13_L1_PER_THREAD   = {C.V13_L1_PER_THREAD:.2f} KB")
    print(f"\n  ROB constants:")
    print(f"    X86_ROB_ENTRY_BYTES  = {C.X86_ROB_ENTRY_BYTES} B")
    print(f"    RISC_ROB_ENTRY_BYTES = {C.RISC_ROB_ENTRY_BYTES} B")
    print(f"    ROB_DENSITY_MULTIPLIER = {C.ROB_DENSITY_MULTIPLIER:.1f}x")
    print(f"\n  Verification: 224 x {C.X86_ROB_ENTRY_BYTES} = {224 * C.X86_ROB_ENTRY_BYTES} SRAM")
    print(f"                784 x {C.RISC_ROB_ENTRY_BYTES} = {784 * C.RISC_ROB_ENTRY_BYTES} SRAM")

    print()
    print("#" * 78)
    print("  v1.3 ROB ENTRY DENSITY BENCHMARK COMPLETE")
    print("#" * 78)


if __name__ == "__main__":
    run()

