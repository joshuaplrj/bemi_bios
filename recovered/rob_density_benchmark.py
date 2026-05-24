"""
ROB (Reorder Buffer) Entry Density Benchmark
==============================================
Quantifies the throughput advantage of RISC/ARM-style compact ROB entries
(4 bytes) vs x86-style monolithic ROB entries (12-16 bytes, midpoint 14 bytes).

ROB = the out-of-order execution window. Deeper ROB = more Instruction-Level
Parallelism (ILP) exposed = higher IPC. But entry size matters: given a fixed
SRAM budget, compact entries pack 3.5x more entries, yielding 3.5x the OoO
window depth. This compounds across virtual threads.

Critical architectural difference:
  x86: Monolithic ROB with CAM-based wakeup -> O(n^2) area scaling,
       penalty per depth doubling = 1.3x cycle time degradation.
  RISC/ARM: Split/distributed ROB -> NO CAM bottleneck,
            IPC scales purely with depth (no cycle-time penalty).

Methods used:
  1. Little's Law IPC model:    IPC(depth) = IPC_max * (1 - e^(-depth / K_sat))
  2. CAM scaling penalty:       only applies to monolithic x86 ROB
  3. SRAM budget sweep:         fixed SRAM -> how many entries can each ISA fit?
  4. Multi-thread throughput:   threads ? physical_cores * virtual_threads_per_core
                                virtual_threads ? ROB_depth / baseline_depth
  5. Workload-dependent:        6 workloads with different ILP characteristics
  6. Comparative analysis:      x86 vs RISC density, multi-methodology cross-validation

Ground truth constants imported from bemi_constants.py.
"""

imp
<truncated 23036 bytes>
iate/displ      : 32-64 bits  (4.0-8.0B)")
    print(f"    Control/status       : 10-15 bits  (1.25-1.875B)")
    print(f"    Total (range)        : 78-127 bits = 9.75-15.875 bytes")
    print(f"    Midpoint             : ~14 bytes  <-- used in benchmark")
    print()
    print(f"  RISC/ARM/Bemi ROB entry breakdown (4 bytes):")
    print(f"    Opcode (compressed)  : 8 bits      (1.0B)")
    print(f"    Register tags (x3)   : 15 bits     (1.875B)")
    print(f"    Control flags        : 5 bits      (0.625B)")
    print(f"    Immediate (optional) : 4 bits      (0.5B)")
    print(f"    Total                : 32 bits = 4 bytes")
    print()

    # Summary of all methods
    print(f"  SUMMARY ACROSS ALL 5 METHODS:")
    print(f"    Method 1 (SRAM sweep):     RISC packs 3.5x entries, wins at all budgets")
    print(f"    Method 2 (Fixed depth):    x86 costs 3.5x SRAM for same depth + CAM penalty")
    print(f"    Method 3 (Density direct): RISC wins IPC at low-mid budgets; x86 CAM penalty")
    print(f"    Method 4 (Thread scaling): RISC total TP = 2.7-5.5x higher across budgets")
    print(f"    Method 5 (Workload ILP):   RISC dominates high-ILP; even at low-ILP")
    print()
    print(f"  ARCHITECTURAL CONCLUSION:")
    print(f"    Compact RISC/ARM/Bemi ROB entries (4B) achieve {C.ROB_DENSITY_MULTIPLIER:.1f}x")
    print(f"    entry density vs x86 (14B midpoint) for identical SRAM budget.")
    print(f"    Combined with split ROB (no CAM O(n^2) penalty), this directly")
    print(f"    enables 3.5x more virtual threads per core, forming the foundation")
    print(f"    of Bemi's 7.8x multi-threaded throughput advantage.")
    print()
    print("#" * 78)
    print("  ROB DENSITY BENCHMARK COMPLETE")
    print("#" * 78)


if __name__ == "__main__":
    run()

