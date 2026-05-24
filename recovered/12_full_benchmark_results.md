# 12. Full Benchmark Results & Analysis

> **Model used:** Bemi v1.2 (144 threads, 4-cyc decode kept, 85W, IPC=1.3/thread)
> **ROB Entry Density variant:** [Bemi v1.3](#1213-v13-rob-entry-density-results) (84 threads, 4B ROB entries, 80W, 109.2 TP)
> For the four-way comparison including Bemi v1.1, see [Chapter 14](14_architecture_version_comparison.md).
> All results reproduced by `bemi_bios/run_all_benchmarks.py` (exit code 0).


All 12 benchmarks are run via:
```
python bemi_bios/run_all_benchmarks.py
```

Architecture parameters (constant across all tests):
- x86: 12 physical cores, 24 threads (2x SMT), 4-cyc decode, 100W TDP
- Bemi: 12 physical cores, 144 threads (12 decoder clusters x 15 RISC units), 4-cyc decode (decoder kept), 85W TDP, 1.3x fusion

---

## 12.1 Benchmark 01 -- MS-DOS 1.0 Legacy OS Boot & Kernel Workload

**File:** `bemi_bios/legacy_os_benchmark.py`
**Source:** `OSEnvironment.simulate_dos_workload()`

### Setup
- 200,000 INT 21h system calls (typical interactive DOS session)
- 50,000 hardware timer interrupts (INT 8h)

### Results

| Platform | INT 21h (cyc) | HW Interrupt (cyc) | Backend TP | Total Wall-Clock |
|---|---|---|---|---|
| Legacy BIOS + Native x86 (24 threads) | 51 | 131 | 6.0 | 3,301,666 |
| Bemi BIOS + Ring -1 DBT + Weaponized (144 threads) | 8 | 20 | 46.8 | 55,555 |

**Speedup: 59.43x**

### Analysis

The 59.43x speedup comes from three compounding f
<truncated 14484 bytes>
c penalty + TAGE pre-fill |
| 10 | TSO Atomic Ops | **Bemi** | 6.54x | 144 threads dominate latency |
| 11 | Memory Hierarchy | **x86** | 5.8x | L1 thinning (2.67 vs 16.0 KB/thread) + memory saturation |
| 12 | Geekbench-Equivalent | **Bemi** | 7.8x (multi) | IPC 1.3x x thread 6.0x |

**Final Score: Bemi 9 / x86 3**

The three x86 wins are architecturally honest:
1. **CISC Dominance (no passthrough):** ASIC hardware beats RISC software loops -- always will.
   Resolved by the Macro-Op Passthrough (Benchmark 06).
2. **Memory Hierarchy:** Thread density physically thins the L1/L2 cache per thread.
   This is a permanent, real cost of packing 6x more execution contexts into the same cache/bandwidth.
3. **CISC Muscles (AVX-512 subset):** Same as CISC Dominance for the vector workload category.

---

## 12.13 v1.3 ROB Entry Density Results

The v1.3 model uses 84 virtual threads (24 x 3.5 ROB density from 4B entries vs x86 14B entries)
with the x86 decoder kept (4-cycle decode, 1.3x fusion). Total throughput: 109.2 (1.3 x 84).

### Parameter Comparison

| Parameter | v1.2 (Weaponized) | v1.3 (ROB Entry Density) |
|---|---|---|
| Thread derivation | 6nm back-end packing | 4B vs 14B ROB entry ratio |
| Virtual threads | 144 | 84 |
| Total throughput | 187.2 | 109.2 |
| TDP | 85W | 80W |
| L1 / thread | 2.67 KB | 4.57 KB |
| ROB structure | Monolithic (inherited) | Split/distributed (no CAM O(n2)) |

### Key Difference

v1.3 achieves its thread density purely from SRAM budget efficiency -- no additional die area
is required. The 3.5x multiplier is the entry size ratio (14/4), not a silicon area multiplier.
This makes v1.3 the most SRAM-efficient architecture in the Bemi family, at the cost of lower
absolute throughput than v1.2 (109.2 vs 187.2).

