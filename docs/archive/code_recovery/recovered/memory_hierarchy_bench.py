"""
Memory Hierarchy Benchmark — Weaponized Bemi (144 threads)
NOTE: 144 threads sharing 12 physical cores' L1/L2 = 2.67 KB L1/thread.
x86 has 16 KB L1/thread. Memory hierarchy is a decisive x86 win here.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from bemi_constants import *
import pandas as pd

L1_LAT, L2_LAT, L3_LAT, RAM_LAT = 4, 12, 40, 200
L3_HIT = 0.50


def simulate_mem(l1_thread_kb, l2_thread_kb, threads, workload_bytes):
    accesses  = workload_bytes / 8
    l1_hit    = min(0.95, (l1_thread_kb / 32.0) * 0.95)
    l2_hit    = min(0.85, (l2_thread_kb / 256.0) * 0.85)
    l1_hits   = accesses * l1_hit
    l2_acc    = accesses - l1_hits
    l2_hits   = l2_acc * l2_hit
    l3_acc    = l2_acc - l2_hits
    l3_hits   = l3_acc * L3_HIT
    ram_acc   = l3_acc - l3_hits
    total_cyc = l1_hits*L1_LAT + l2_hits*L2_LAT + l3_hits*L3_LAT + ram_acc*RAM_LAT
    avg_lat   = total_cyc / accesses
    # Memory-bound workloads saturate the memory subsystem well before 144-way
    # virtual threading can linearly scale. Model effective parallelism as
    # capped at ~2 threads per physical core.
    effective_parallelism = min(threads, PHYSICAL_CORES * 2)
    wall      = total_cyc / effective_parallelism
    return avg_lat, wall, l1_hit, l2_hit


def run():
    wb = 100 * 10 ** 6
    # L1/L2 per thread
    x86_l1  = (L1_PER_CORE_KB   * PHYSICAL_CORES) / X86_THREADS    # 16.0 KB
    x86_l2  = (L2_PER_CORE_KB   * PHYSICAL_CORES) / X86_THREADS    # 256.0 KB
    bemi_l1 = (L1_PER_CORE_KB   * PHYSICAL_CORES) / BEMI_THREADS   # 2.67 KB
    bemi_l2 = (L2_PER_CORE_KB   * PHYSICAL_CORES) / BEMI_THREADS   # 42.7 KB

    print("=" * 75)
    print("  Memory Hierarchy & Cache Contention Benchmark")
    print("=" * 75)
    print(f"  x86  L1/thread: {x86_l1:.1f} KB  | L2/thread: {x86_l2:.1f} KB ({X86_THREADS} threads)")
    print(f"  Bemi L1/thread: {bemi_l1:.2f} KB | L2/thread: {bemi_l2:.1f} KB ({BEMI_THREADS} threads)")
    print(f"  Bemi's 144 threads share the SAME physical cache -> L1 per thread is 6x thinner")
    print()

    rows = []
    for name, l1, l2, threads in [
        (f"Native x86 ({X86_THREADS} threads)",  x86_l1,  x86_l2,  X86_THREADS),
        (f"Bemi Weaponized ({BEMI_THREADS} threads)", bemi_l1, bemi_l2, BEMI_THREADS),
        ("Hybrid Bemi (DBT cache pressure)",        bemi_l1*0.75, bemi_l2*0.75, BEMI_THREADS),
    ]:
        al, wc, l1h, l2h = simulate_mem(l1, l2, threads, wb)
        rows.append({"Architecture": name, "L1 Hit": f"{l1h*100:.1f}%",
                     "L2 Hit": f"{l2h*100:.1f}%",
                     "Avg Lat (cyc)": round(al, 2),
                     "Wall-Clock (rel)": int(wc)})

    print(pd.DataFrame(rows).to_string(index=False))
    print()
    print("  x86 wins decisively: 16 KB L1/thread vs Bemi's 2.67 KB L1/thread.")
    print("  This is the primary honest cost of 6x thread density on the same die.")
    print("  More threads = less cache per thread = more L3/DRAM latency.")


if __name__ == "__main__":
    run()
