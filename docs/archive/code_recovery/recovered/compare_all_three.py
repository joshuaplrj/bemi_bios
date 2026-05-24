"""
Three-Way Benchmark Comparison
================================
  A) Native x86            : 24 threads, 4-cyc decode, 100W, IPC=1.0
  B) Bemi v1.1             : 36 threads, 1-cyc decode, 65W,  IPC=5.2  (ROB density model)
  C) Bemi v1.2             : 144 threads, 4-cyc decode, 85W, IPC=1.3  (6nm RISC size model)
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

ARCHS = {
    "x86 (Native)":       {"T": 24,  "D": 4, "F": 1.0, "W": 100.0, "L1": 16.00},
    "Bemi v1.1 (36T,1cyc)":{"T": 36,  "D": 1, "F": 1.3, "W": 65.0,  "L1": 10.67},
    "Bemi v1.2 (144T,4cyc)":{"T": 144, "D": 4, "F": 1.3, "W": 85.0,  "L1": 2.67},
}

PHYS_CORES = 12
L2_PER_CORE_KB = 512
L1_PER_CORE_KB = 32

SEP = "-" * 80

def ipc(a):  return (4 / a["D"]) * a["F"]
def tp(a):   return ipc(a) * a["T"]

def hdr(title):
    print(f"\n{'='*80}")
    print(f"  {title}")
    print('='*80)
    print(f"  {'Architecture':<25} {'Threads':>7} {'Decode':>7} {'IPC/T':>6} {'TotalTP':>8}")
    print(SEP)
    for n,a in ARCHS.items():
        print(f"  {n:<25} {a['T']:>7} {a['D']:>6}c {ipc(a):>6.2f} {tp(a):>8.1f}")
    print()

def row(label, vals, unit="", fmt=".2f", winner_min=True):
    line = f"  {label:<35}"
    nums = [float(v) for v in vals]
    best = min(nums) if winner_min else max(nums)
    for v in nums:
        tag = " <--" if v == best else "    "
        line += f"  {v:{fmt}}{unit}{tag}"
    print(
<truncated 10465 bytes>
86':>15} {'Bemi v1.1':>15} {'Bemi v1.2':>15}")
print(SEP)
params_compare = [
    ("Threads",        24,    36,    144),
    ("Decode Latency", "4 cyc","1 cyc","4 cyc (kept)"),
    ("IPC / thread",   "1.0x","5.2x","1.3x"),
    ("Total Throughput","24.0","187.2","187.2"),
    ("TDP (W)",        100,   65,    85),
    ("L1 / thread (KB)", "16.0", "10.7", "2.67"),
    ("Single-core IPC", "1.0x","5.2x","1.3x"),
    ("Multi-core adv.", "1.0x","7.8x","7.8x"),
]
for row_data in params_compare:
    label = row_data[0]
    vals  = row_data[1:]
    print(f"  {label:<30} {str(vals[0]):>15} {str(vals[1]):>15} {str(vals[2]):>15}")

print(f"\n{'#'*80}")
print("  KEY DIFFERENCES: Bemi v1.1 vs Bemi v1.2")
print(f"{'#'*80}")
print("""
  Old Bemi -> Bemi v1.1 (36T, 1-cyc decode):
    + Better single-thread IPC: 5.2x vs x86 (decode savings compound per thread)
    + Better L1 per thread: 10.7 KB (vs Bemi v1.2's 2.67 KB)
    + Lower TDP: 65W (decoder removed entirely)
    - Fewer threads: only 36 (vs 144)
    - Assumes x86 decoder IS removed -> not Weaponized (loses fusion hardware)

  New Bemi -> Bemi v1.2 (144T, 4-cyc decode):
    + Far more threads: 144 (6nm RISC back-end physically fits 6x more units)
    + Keeps x86 decoder: TAGE predictor, macro-op fusion, enterprise predictors
    - Same IPC per thread as x86: 1.3x (only fusion bonus, no decode savings)
    - More cache pressure: 2.67 KB L1/thread (6x thinner than x86)
    - Higher TDP: 85W (decoder still burns power)

  Both achieve identical multi-core throughput (187.2 total TP) via different routes.
  Choice depends on workload:
    - Latency-sensitive / single-threaded -> Bemi v1.1 wins (5.2x IPC)
    - Throughput-bound + keeps x86 HW ecosystem -> Bemi v1.2 wins (6x threads, same silicon)
""")
