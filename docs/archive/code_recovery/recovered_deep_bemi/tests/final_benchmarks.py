"""
Final Comprehensive Benchmark (4 Workloads)
=============================================
Weaponized Bemi: 144 threads, decode=4 (x86 decoder KEPT), IPC=1.3, TDP=85W.

Cycle model:
  x86  : 4 decode + N execute = (4+N) cycles
  Bemi : 4 decode + N execute = (4+N) cycles  [same decode — decoder is KEPT]

Bemi's advantage is THREAD DENSITY (144 vs 24), not decode latency.
On passthrough ops (AVX/AES/MOVSB), the Macro-Op still goes through the
x86 decoder (1 cycle of the 4-cycle pipeline) but the net cycle count is the
same for both. Thread density is the sole differentiator.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from bemi_constants import *

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

ARTIFACT_DIR = os.path.join(os.path.dirname(__file__), "..", "artifacts")

WORKLOADS = [
    "General Integer Math",
    "AVX-512 (Passthrough)",
    "AES-NI (Passthrough)",
    "REP MOVSB (Passthrough)",
]

# Cycle counts: both architectures use the same x86 decoder and same ASICs.
# The ONLY difference is thread count (24 vs 144) and fusion bonus (1.0 vs 1.3).
X86_CYCLES  = [5, 8, 8, 6]   # 4 decode + N execute
BEMI_CYCLES = [5, 8, 8, 6]   # SAME — decoder kept, same ASIC execute


def run_comprehensive_benchmarks():
    x86_ticks  = [c / X86_THREADS  for c in X86_CYCLES]
    # Bemi: same cycles but
<truncated 1091 bytes>
ticks[i], 4),
            "Speedup": f"{speedup:.2f}x",
            "x86 Energy": round(x86_energy[i], 4),
            "Bemi Energy": round(bemi_energy[i], 4),
            "Eff Gain": f"{eff:.2f}x",
        })
    print(pd.DataFrame(rows).to_string(index=False))
    print()
    print(f"  Cycle model note: x86 and Bemi have IDENTICAL cycle counts.")
    print(f"  Bemi wins entirely from {BEMI_THREADS} threads / ({X86_THREADS} x {BEMI_FUSION}) = {BEMI_THREADS*BEMI_FUSION/X86_THREADS:.2f}x net advantage.")

    # Chart
    x = np.arange(len(WORKLOADS))
    w = 0.35
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    ax1.bar(x - w/2, x86_ticks,  w, label=f"x86 ({X86_THREADS} threads)", color="#d62728", alpha=0.9)
    ax1.bar(x + w/2, bemi_ticks, w, label=f"Bemi ({BEMI_THREADS} threads, 1.3x fusion)", color="#2ca02c", alpha=0.9)
    ax1.set_title("Execution Time (lower is better)")
    ax1.set_xticks(x); ax1.set_xticklabels(WORKLOADS, rotation=15, ha='right', fontsize=9)
    ax1.set_ylabel("Wall-Clock Ticks"); ax1.legend()

    ax2.bar(x - w/2, x86_energy,  w, label=f"x86 ({X86_TDP}W)", color="#d62728", alpha=0.9)
    ax2.bar(x + w/2, bemi_energy, w, label=f"Bemi ({BEMI_TDP}W)", color="#2ca02c", alpha=0.9)
    ax2.set_title("Energy (lower is better)")
    ax2.set_xticks(x); ax2.set_xticklabels(WORKLOADS, rotation=15, ha='right', fontsize=9)
    ax2.set_ylabel("Relative Energy"); ax2.legend()

    plt.suptitle("Weaponized Bemi: Thread Density (144 vs 24) + Fusion (1.3x)", fontsize=11)
    plt.tight_layout()
    os.makedirs(ARTIFACT_DIR, exist_ok=True)
    p = os.path.join(ARTIFACT_DIR, "final_benchmark_chart.png")
    plt.savefig(p, dpi=150, bbox_inches="tight"); plt.close()
    print(f"\n  Chart saved -> {p}")


if __name__ == "__main__":
    run_comprehensive_benchmarks()
