"""
Arithmetic & Memory Benchmark
================================
Corrected for Weaponized Bemi physical thread count.

THREAD COUNT DERIVATION (6nm):
  RISC execution back-end: ~0.15 mm² (20x smaller than x86 back-end)
  x86 decoder kept (weaponized for fusion): 0.75 mm² per cluster
  Available back-end area per cluster: 2.25 mm²
  RISC units per cluster: 2.25/0.15 = 15
  12 clusters x 15 x 0.85 (overhead) = 144 threads

DECODE LATENCY:
  Weaponized Bemi KEEPS the x86 decoder -> decode = 4 cycles (same as x86).
  IPC advantage comes ONLY from macro-op fusion: (4/4) x 1.3 = 1.3/thread.

RISC INSTRUCTION EXPANSION:
  Arithmetic: 1.5x (CISC ADD [mem],reg -> RISC Load+ADD+Store)
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from bemi_constants import *
import pandas as pd


def run():
    total_ops = 10 ** 9

    # x86: 1.0 ops/cyc/thread, 24 threads, no expansion
    x86_throughput = X86_IPC * X86_THREADS                    # 24.0
    x86_time       = total_ops / x86_throughput

    # Bemi: 1.3 ops/cyc/thread (fusion only), 144 threads, 1.5x expansion
    bemi_throughput = BEMI_IPC * BEMI_THREADS                  # 187.2
    bemi_time       = (total_ops * ARITH_EXPANSION) / bemi_throughput

    # L1 instruction cache per thread (same physical cache, more threads)
    x86_l1_hit  = min(0.95, (X86_L1_PER_THREAD  / 32.0) * 0.95)
    bemi_l1_hit = min(0.95, (BEMI_L1_PER_THREAD
<truncated 217 bytes>
=" * 65)
    print(f"  Arithmetic ops (high-level): {total_ops:,}")
    print(f"  Bemi instruction expansion : {ARITH_EXPANSION}x (RISC: load + op + store)")
    print(f"  x86  IPC/thread={X86_IPC:.1f}  threads={X86_THREADS}  total-TP={x86_throughput:.1f}")
    print(f"  Bemi IPC/thread={BEMI_IPC:.1f}  threads={BEMI_THREADS} total-TP={bemi_throughput:.1f}")
    print(f"  (Bemi decode=4 cyc same as x86; advantage is THREAD DENSITY, not decode)")
    print()

    results = [
        {"Architecture": "Native x86 (CISC)",
         "Threads": X86_THREADS, "Instr Expansion": f"{1.0}x",
         "Arith Throughput": round(x86_throughput, 1),
         "Arith Time (rel)": round(x86_time, 2),
         "L1i Hit Rate": f"{x86_l1_hit*100:.1f}%",
         "Mem Bandwidth": round(x86_bw, 2)},
        {"Architecture": "Bemi (Weaponized, 144 threads)",
         "Threads": BEMI_THREADS, "Instr Expansion": f"{ARITH_EXPANSION}x",
         "Arith Throughput": round(bemi_throughput, 1),
         "Arith Time (rel)": round(bemi_time, 2),
         "L1i Hit Rate": f"{bemi_l1_hit*100:.1f}%",
         "Mem Bandwidth": round(bemi_bw, 2)},
    ]
    print(pd.DataFrame(results).to_string(index=False))
    print()

    speedup = x86_time / bemi_time
    print(f"  Bemi is {speedup:.2f}x faster on integer arithmetic.")
    print(f"  Source: {BEMI_THREADS} threads x {BEMI_IPC} IPC = {bemi_throughput} TP")
    print(f"          vs {X86_THREADS} threads x {X86_IPC} IPC = {x86_throughput} TP")
    print(f"          Bemi pays 1.5x expansion but 7.8x more thread-TP -> net {speedup:.2f}x")
    print()
    print(f"  Note: Bemi L1 per thread = {BEMI_L1_PER_THREAD:.1f} KB vs x86's {X86_L1_PER_THREAD:.1f} KB")
    print(f"  144 threads share the same 384 KB L1 pool -> cache pressure is real.")


if __name__ == "__main__":
    run()
