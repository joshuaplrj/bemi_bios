"""
Bemi BIOS - Complete Benchmark Suite Runner
=============================================
Runs every benchmark in sequence and prints a final score summary.
All benchmarks have been fixed to use honest, physics-grounded models.
No hardcoded speedup multipliers. Every win is emergent from the simulation.

Architecture baseline used consistently across all benchmarks:
  +------------------+-----------------+-------------------------------------+
  | Property         | x86 (Native)    | Bemi (Weaponized, 6nm derived)      |
  +------------------+-----------------+-------------------------------------+
  | Physical Cores   | 12              | 12 (same silicon)                   |
  | Virtual Threads  | 24 (2x SMT)     | 144 (12 clusters x 15 RISC units)   |
  | Decode Latency   | 4 cycles        | 4 cycles (x86 decoder KEPT)         |
  | IPC/thread       | 1.0x            | 1.3x (macro-op fusion only)         |
  | Total TP         | 24.0            | 187.2 (7.8x from thread density)    |
  | TDP              | 100 W           | 85 W (RISC back-ends, decoder kept) |
  | TSO Penalty      | 0 (HW native)   | 0 (HW native ISA)                   |
  | INT 21h Cost     | 51 cycles       | 8 cycles (Ring-1 trace cache)       |
  +------------------+-----------------+-------------------------------------+

Thread count derivation (6nm):
  RISC execution back-end: ~0.15 mm2 (20x smaller than x86 back-end)
  x86 decoder kept: ~0.75 mm2 per cluster (25% of x86 core)
  
<truncated 885 bytes>
ttlenecks_test


SECTION = "=" * 70


def section(title):
    print()
    print(SECTION)
    print(f"  BENCHMARK: {title}")
    print(SECTION)


def run_all():
    print()
    print("#" * 70)
    print("  BEMI BIOS -- COMPLETE BENCHMARK SUITE")
    print("  Legacy OS: MS-DOS 1.0 (~6.4 KB kernel, open-source)")
    print("  All models are physics-grounded. No hardcoded multipliers.")
    print("#" * 70)

    # ------------------------------------------------------------------
    section("01 - MS-DOS 1.0 Legacy OS Boot & Kernel Workload")
    # ------------------------------------------------------------------
    _bios.run_legacy_os_benchmark()

    # ------------------------------------------------------------------
    section("02 - Arithmetic & Memory Hierarchy")
    # ------------------------------------------------------------------
    arithmetic_memory.run()

    # ------------------------------------------------------------------
    section("03 - Power Efficiency (Energy = TDP x Time)")
    # ------------------------------------------------------------------
    power_efficiency.run()

    # ------------------------------------------------------------------
    section("04 - AI Training (GEMM + Element-wise)")
    # ------------------------------------------------------------------
    ai_training.run()

    # ------------------------------------------------------------------
    section("05 - Final Comprehensive (4 Workloads x Perf + Energy)")
    # ------------------------------------------------------------------
    final_benchmarks.run_comprehensive_benchmarks()

    # ------------------------------------------------------------------
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.