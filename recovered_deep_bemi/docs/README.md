# Bemi BIOS -- Complete Technical Documentation

This folder contains the definitive, end-to-end technical documentation for the **Bemi BIOS** project.
It covers the full engineering arc: from the original x86 problem statement, through every architectural
iteration, to the final honest benchmark suite validated against **MS-DOS 1.0** as a real legacy OS workload.

---

## Architecture Version Summary

| Property | x86 (Baseline) | Bemi v1.1 | Bemi v1.2 (HW spec) | Bemi v1.3 (ROB Density) | Bemi v2.0 (Scaled Dom.) | Bemi v3.0 (Mem & Pred) | Bemi v4.0 (Zenith) |
|---|---|---|---|---|---|---|---|
| Implementation | Silicon native | Compiler-native ISA | x86 decoder kept | ROB density (4B entry) | L0, independent ROB, MLP | 128MB L4, HMC, PTC | Adaptive HMC, 256MB L4, NPP, DCF |
| Physical Cores | 12 | 12 | 12 | 12 (host) | 12 | 12 | 12 |
| Virtual Threads | 24 | **36** (3x ROB density) | **144** (6nm RISC size) | **84** (24 x 3.5 density) | **48** (SMT-4) | **60** (SMT-5) | **72** (SMT-6 / 36 Fused) |
| Decode Latency | 4 cyc | **1 cyc** (fixed-32) | 4 cyc (decoder KEPT) | 4 cyc (decoder kept) | 4 cyc (decoder kept) | 1.75 cyc (blended PTC) | 1.35 cyc (blended NPP) |
| IPC / thread (peak) | 1.0x | **5.2x** (decode + fusion) | 1.3x (fusion only) | 1.3x (fusion only) | 1.5x (6-pair fusion) | 3.66x (8-pair fusion) | 5.18x (10-pair fusion) |
| Total Throughput | 24.0 | 187.2 | 187.2 | 109.2 (1.3 x 84) | 72.0 | 219.6 | 373.3 (SMT) / 186.7 (Fused) |
| TDP | 100 W | **65 W** (decoder removed) | 85 W 
<truncated 6091 bytes>
uted from the **repository root folder**:

```bash
# Full benchmark suite (includes x86 vs v1.2, v1.3, v2.0, v3.0, and v4.0 summary):
python bemi_bios/run_all_benchmarks.py

# Three-way comparison (x86 vs v1.1 vs v1.2):
python compare_all_three.py

# v1.3 ROB Entry Density parameter sweeps and workload simulation:
python tests/rob_density_benchmark.py
python tests/rob_dbt_benchmarks.py

# Grounded Model Scaling Progression (x86 vs v1.3 Grounded vs v2.0 vs v3.0 vs v4.0):
python tests/scaling_bottlenecks_test.py

# Run the complete test suite (includes all individual tests):
python run_all_tests.py
```

---

## Final Scores

### v1.3 ROB Entry Density (84-thread model)

| Metric | Value |
|---|---|
| ROB entry size | 4 bytes (vs x86 14 bytes; 3.5x density) |
| Virtual threads | 84 (24 baseline x 3.5 density) |
| Total throughput | 109.2 (1.3 IPC x 84 threads) |
| TDP | 80 W |
| L1 per thread | 4.57 KB |

### v1.3 ROB Entry Density (10 ROB density benchmarks)

| Winner | Count | Key mechanism |
|---|---|---|
| Bemi v1.3 | 10 / 10 | 84 threads x 1.3 IPC = 109.2 TP; 4B ROB entries = 3.5x density |
| Native x86 | 0 / 10 | ROB entries are 14 bytes vs Bemi's 4 bytes |

x86 loses all 10 ROB density benchmarks because Bemi v1.3 packs 3.5x more entries
into the same SRAM budget without the CAM O(n^2) scaling penalty of a monolithic ROB.

### v1.2 Model (12-benchmark suite)

| Winner | Count | Key mechanism |
|---|---|---|
| Bemi v1.2 | 9 / 12 | 144 threads x 1.3 IPC = 187.2 TP; Ring -1 trace cache |
| Native x86 | 3 / 12 | Dedicated ASIC (no passthrough); 16 KB L1/thread vs 2.67 KB |

The three x86 wins are architecturally honest. See Chapter 14 for v1.1's different loss profile.

