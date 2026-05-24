"""
Geekbench-Equivalent Benchmark
==================================
Weaponized Bemi: 144 threads, decode=4 (kept), IPC=1.3/thread (fusion only).
Single-core: only 1.3x better (fusion). Multi-core: 7.8x better (thread density).
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from bemi_constants import *
import pandas as pd

NORM = 400


def run():
    x86_sc  = X86_IPC  * NORM
    bemi_sc = BEMI_IPC * NORM
    x86_mc  = X86_IPC  * X86_THREADS  * NORM
    bemi_mc = BEMI_IPC * BEMI_THREADS * NORM

    print("=" * 65)
    print("  Bemi-Bench (Geekbench-Equivalent)")
    print("=" * 65)
    print(f"  IPC = (issue={ISSUE_WIDTH} / decode) x fusion")
    print(f"  x86  IPC = ({ISSUE_WIDTH}/{X86_DECODE}) x {X86_FUSION} = {X86_IPC:.2f}")
    print(f"  Bemi IPC = ({ISSUE_WIDTH}/{BEMI_DECODE}) x {BEMI_FUSION} = {BEMI_IPC:.2f}  [decode same; fusion only]")
    print()

    rows = [
        {"Architecture": "Native x86 (CISC)",
         "Threads": X86_THREADS, "IPC": X86_IPC,
         "Single-Core": int(x86_sc), "Multi-Core": int(x86_mc)},
        {"Architecture": f"Bemi Weaponized ({BEMI_THREADS} threads)",
         "Threads": BEMI_THREADS, "IPC": BEMI_IPC,
         "Single-Core": int(bemi_sc), "Multi-Core": int(bemi_mc)},
    ]
    print(pd.DataFrame(rows).to_string(index=False))
    print()
    print(f"  Single-core advantage : {bemi_sc/x86_sc:.2f}x  (fusion only: decode unchanged)")
    print(f"  Multi-core  advantage : {bemi_mc/x86_mc:.2f}x  (1.3 IPC x {BEMI_THREADS/X86_THREADS:.1f}x threads)")
    print()
    print(f"  KEY INSIGHT: Weaponized Bemi is a THROUGHPUT machine.")
    print(f"  Single-thread latency = only 1.3x better (not 5.2x).")
    print(f"  Throughput-bound workloads = 7.8x better (from 6x more threads).")


if __name__ == "__main__":
    run()
