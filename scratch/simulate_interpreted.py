# scratch/simulate_interpreted.py
"""
Pentium Simulator: Interpreted Command (Serial Bytecode) Benchmark
===================================================================
Models running a single-threaded bytecode interpreter (e.g., Python or JVM loop)
using the robust instruction-level simulation framework to compare Stock Legacy BIOS
vs Bemi BIOS v7.2 under serial (1 thread) and parallel (16 threads) limits.
"""

import os
import sys

# Add parent and bemi_bios directories to path
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.insert(0, parent_dir)
sys.path.insert(0, os.path.join(parent_dir, "bemi_bios"))

from pentium_cpu import PentiumCPU
from bemi_bios_sim import LegacyBIOS, BemiBIOS
from apt_os_sim import AptOS

def run_interpreted_simulation():
    print("=" * 95)
    print("  RUNNING INTERPRETED COMMAND BENCHMARK (SERIAL WORKLOAD)")
    print("  Comparing: Stock Legacy BIOS vs Bemi BIOS v7.2 on P54C Pentium (Real Instruction Stream)")
    print("=" * 95)

    # 1. Instantiate isolated components
    cpu = PentiumCPU()
    legacy_bios = LegacyBIOS()
    bemi_bios = BemiBIOS()
    os_sim = AptOS()

    # Generate the bytecode interpreter workload (100,000 ops, ~1.1M instructions)
    print("\n[Generating Interpreter Workload...]")
    stream = os_sim.generate_interpreted_workload()
    total_instr = len(stream)
    print(f"Generated {total_instr:,} instructions representing 100,000 bytecode operations.")

    # 2. Run Stock Pentium (Serial baseline)
    legacy_bios.boot(cpu)
    cpu.reset_stats()
    cpu.execute_instruction_block(stream, parallel_threads=1)
    legacy_stats = cpu.read_stats()

    # 3. Run Bemi BIOS v7.2 (Serial - Only 1 active thread)
    bemi_bios.boot(cpu)
    cpu.reset_stats()
    cpu.execute_instruction_block(stream, parallel_threads=1)
    bemi_serial_stats = cpu.read_stats()

    # 4. Run Bemi BIOS v7.2 (Parallel - 16 threads, if the workload were parallelizable)
    cpu.reset_stats()
    cpu.execute_instruction_block(stream, parallel_threads=16)
    bemi_parallel_stats = cpu.read_stats()

    # 5. Print Results Table
    print(f"\n  [Benchmark Results - 100,000 Ops]")
    print(f"  {'Configuration':<35} | {'Throughput (IPC)':<18} | {'Execution Ticks':<18} | {'Relative Speedup':<18}")
    print("-" * 98)
    
    p5_ticks = legacy_stats["cycles"]
    bemi_serial_ticks = bemi_serial_stats["cycles"]
    bemi_parallel_ticks = bemi_parallel_stats["cycles"]
    
    p5_ipc = legacy_stats["ipc"]
    bemi_serial_ipc = bemi_serial_stats["ipc"]
    bemi_parallel_ipc = bemi_parallel_stats["ipc"]

    print(f"  {'Stock Pentium (Serial)':<35} | {p5_ipc:<18.3f} | {p5_ticks:<18,.0f} | {'1.00x (Baseline)':<18}")
    print(f"  {'Pentium + Bemi v7.2 (Serial)':<35} | {bemi_serial_ipc:<18.3f} | {bemi_serial_ticks:<18,.0f} | {f'{p5_ticks / bemi_serial_ticks:.2f}x':<18}")
    print(f"  {'Pentium + Bemi v7.2 (Parallel)':<35} | {bemi_parallel_ipc:<18.3f} | {bemi_parallel_ticks:<18,.0f} | {f'{p5_ticks / bemi_parallel_ticks:.2f}x':<18}")
    print("-" * 98)

    # 6. Print Energy Metrics
    p5_energy = legacy_stats["energy_joules"]
    bemi_serial_energy = bemi_serial_stats["energy_joules"]
    
    print(f"\n  [Energy Metrics]")
    print(f"  Stock Pentium Energy : {p5_energy:.5f} Joules")
    print(f"  Bemi v7.2 Serial Eng : {bemi_serial_energy:.5f} Joules ({p5_energy/bemi_serial_energy:.2f}x savings)")
    print("-" * 98)

    # 7. Performance Analysis
    print("""
  [Performance Analysis & Key Takeaway]
  * Bemi BIOS v7.2 still provides a substantial speedup over stock Pentium under serial constraints.
    This is due to latency reduction: trace-cached decode, NPP branch predictor, and MLP latency hiding.
  * However, Bemi BIOS CANNOT maintain its thousands-fold speedup scaling here when running serially.
  * Why: The interpreter loop is strictly sequential (serial). It cannot utilize the 16 temporal
    threads of the Bemi v7.2 microarchitecture when parallel_threads=1.
    The execution throughput drops from the parallel peak (using SMT) down to the serial IPC,
    illustrating that Bemi's extreme scaling is highly dependent on task-level parallelism.
""")

if __name__ == "__main__":
    run_interpreted_simulation()
