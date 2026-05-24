"""
Optimized x86 Bemi Benchmark
==============================
Overall performance comparison: x86 vs Weaponized Bemi.

Uses bemi_constants.py as single source of truth for v1.2 params.
- x86: 24 threads, IPC=1.0, native TSO, 16-cyc branch penalty
- Bemi: 144 threads, IPC=1.3, native TSO, 8-cyc branch penalty, TAGE prefilled
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from bemi_constants import *
import pandas as pd


def simulate_arch(name, threads, ipc, tso_penalty, branch_penalty,
                  indirect_mult, cache_pressure, l1_total, l2_total):
    workload = 10 ** 7

    # TSO / Atomics
    atomic_base = 25
    effective_atomic = atomic_base + tso_penalty + (threads * 0.5)
    atomic_throughput = threads / effective_atomic

    # Branch Prediction
    branch_miss_rate = 0.05
    direct_time = workload * 0.8 * branch_miss_rate * branch_penalty
    indirect_time = workload * 0.2 * branch_miss_rate * (branch_penalty * indirect_mult)
    branch_overhead = (direct_time + indirect_time) / workload

    # Memory & Cache
    l1_per_thread = l1_total / threads
    hit_rate = min(0.95, (l1_per_thread / 32.0) * 0.95) * cache_pressure
    miss_penalty = (1 - hit_rate) * 40
    memory_latency = 4 + miss_penalty

    # Aggregate effective throughput
    effective_ipc = ipc / (1 + branch_overhead + (memory_latency / 100))
    total_throughput = effective_ipc * threads

    return atomic_throughput, memory_latency, total_throughput


def run():
    x86_at, x86_ml, x86_agg = simulate_arch(
        "Native x86", X86_THREADS, X86_IPC, X86_TSO_PENALTY,
        X86_BRANCH_PENALTY, X86_INDIRECT_MULT, 1.0,
        L1_PER_CORE_KB * PHYSICAL_CORES, L2_PER_CORE_KB * PHYSICAL_CORES,
    )
    bemi_at, bemi_ml, bemi_agg = simulate_arch(
        "Weaponized Bemi", BEMI_THREADS, BEMI_IPC, BEMI_NATIVE_TSO,
        BEMI_BRANCH_PENALTY, BEMI_INDIRECT_MULT, 1.0,
        L1_PER_CORE_KB * PHYSICAL_CORES, L2_PER_CORE_KB * PHYSICAL_CORES,
    )

    print("--- Weaponized x86 Bemi Simulation ---")
    results = [
        {"Architecture": "Native x86", "Atomic TP": round(x86_at, 2),
         "Mem Latency": round(x86_ml, 2), "Agg Throughput Score": round(x86_agg, 2)},
        {"Architecture": "Weaponized Bemi", "Atomic TP": round(bemi_at, 2),
         "Mem Latency": round(bemi_ml, 2), "Agg Throughput Score": round(bemi_agg, 2)},
    ]
    print(pd.DataFrame(results).to_string(index=False))


if __name__ == "__main__":
    run()
