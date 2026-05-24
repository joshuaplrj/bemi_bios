"""
CISC Dominance & TSO & Memory & Branch benchmarks
===================================================
All updated for Weaponized Bemi: 144 threads, decode=4, IPC=1.3/thread.
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


def run_benchmarks():
    workloads = ["AVX-512 (Vector Math)", "AES-NI (Crypto)", "REP MOVSB (String Copy)"]

    # x86: 4 decode + N execute on dedicated hardware
    x86_exec  = [4, 4, 2]
    x86_wall  = [(X86_DECODE + e) / X86_THREADS for e in x86_exec]

    # Bemi WITHOUT passthrough: software RISC loops, 4 decode (x86 kept) + many RISC ops
    bemi_exec = [64, 120, 6]  # software emulation
    bemi_wall = [(BEMI_DECODE + e) / (BEMI_THREADS * BEMI_FUSION) for e in bemi_exec]

    print("=" * 70)
    print("  CISC Dominance Benchmark (Bemi WITHOUT Macro-Op Passthrough)")
    print("=" * 70)
    print(f"  x86  : {X86_THREADS} threads, {X86_DECODE}-cyc decode, dedicated ASIC hardware")
    print(f"  Bemi : {BEMI_THREADS} threads, {BEMI_DECODE}-cyc decode, RISC software loop")
    print(f"  Effective Bemi TP: {BEMI_THREADS}x{BEMI_FUSION}={BEMI_THREADS*BEMI_FUSION:.0f} vs x86's {X86_THREADS}")
    print()

    header = f"{'Workload':<25} | {'x86 Ticks':>10} | {'Bemi Ticks':>10} | {'Winner':>15}"
    print(header); print("-" * len(header))
    for w, x, b in zip(workloads, x86_wall, bemi_wall):
        winner = f"x86 ({b/x:.1f}x)" if x < b else f"Bemi ({x/b:.1f}x)"
        print(f"{w:<25} | {x:>10.4f} | {b:>10.4f} | {winner:>15}")

    print()
    print("  Even with 144 threads, software RISC loops (64-120 ops) cannot beat")
    print("  dedicated ASIC silicon (4 ops). Passthrough resolves this.")

    x = np.arange(len(workloads)); width = 0.35
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(x - width/2, x86_wall,  width, label=f"x86 ({X86_THREADS} threads, dedicated HW)", color="#1f77b4", alpha=0.9)
    ax.bar(x + width/2, bemi_wall, width, label=f"Bemi ({BEMI_THREADS} threads, RISC sw loop)", color="#ff7f0e", alpha=0.9)
    ax.set_title("CISC Dominance: x86 Dedicated Silicon vs RISC Software (No Passthrough)", fontsize=11)
    ax.set_xticks(x); ax.set_xticklabels(workloads); ax.legend()
    ax.set_ylabel("Wall-Clock Ticks (lower is better)")
    plt.tight_layout()
    os.makedirs(ARTIFACT_DIR, exist_ok=True)
    p = os.path.join(ARTIFACT_DIR, "cisc_wins_chart.png")
    plt.savefig(p, dpi=150, bbox_inches="tight"); plt.close()
    print(f"\n  Chart saved -> {p}")


if __name__ == "__main__":
    run_benchmarks()
