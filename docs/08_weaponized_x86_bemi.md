# 08. Bemi v1.2 -- Weaponized x86 Bemi

> **Architecture version:** Bemi v1.2 | Predecessor: [Bemi v1.1 (Chapter 07)](07_native_isa_evolution.md)
> This is the current hardware architecture specification. For the ROB Entry Density variant see [Bemi v1.3](#813-bemi-v13-rob-entry-density-derivation) below.
> For parameter comparison across all versions see [Chapter 14](14_architecture_version_comparison.md).

## 8.1 What "Weaponized" Means

"Weaponized x86 Bemi" (Bemi v1.2) is not merely a branding term. It describes a specific
architectural posture that emerged from the physical reality of 6nm silicon: RISC execution
back-ends are **10-40x smaller** than x86 cores at 6nm, so keeping the x86 decoder and filling
the freed back-end area with many small RISC execution units yields far more threads than
removing the decoder and deepening the ROB (v1.1's approach).

Bemi v1.1 (Chapter 07) derived its advantage from **removing the decoder** -- 1-cycle decode,
5.2x IPC, 65W TDP, 36 threads. Bemi v1.2 takes the opposite posture: the decoder is KEPT
as a macro-op fusion engine, and the die area savings come from replacing the expensive x86
execution back-end with many small RISC units.

**The key v1.2 thread count derivation (6nm):**

```
RISC execution back-end : 0.15 mm?  (20x smaller than x86 back-end at 2.25 mm?)
x86 front-end decoder   : 0.75 mm?  (kept, 25% of 3 mm? x86 core)
Available per cluster   : 2.25 mm? / 0.15 mm? = 15 RISC units per decoder cluster
12 clusters x 15 x 0.85 overhead = 144 virtual threads
```

**Compare with v1.1 (ROB density):**
```
Decoder removed         : saves ~0.75 mm? per core
Freed area per core     : 2.25 mm? available for ROB depth
ROB depth multiplier    : 3x deeper -> 3 virtual windows per core
12 cores x 3           = 36 virtual threads
```

The key observation: x86 processors have the most sophisticated instruction-level intelligence
in consumer computing history. Their TAGE branch predictors, macro-op fusion engines, hardware
prefetchers, and out-of-order schedulers took 40 years and hundreds of billions of dollars to develop.

Weaponized Bemi's thesis is: **keep the x86 front-end decoder (and its fusion/branch intelligence),
but replace the expensive x86 execution back-end with many smaller RISC execution units.** This gives you:
- The x86 decoder is **kept** -> decode latency stays **4 cycles** (same as x86)
- Macro-op fusion bonus (+30% effective IPC)
- Massive thread density from back-end area packing (144 threads vs 24)
- Lower TDP than baseline x86 despite keeping the decoder (85W vs 100W)

---

## 8.2 The Three Weaponized Advantages

### Advantage 1: Macro-Op Fusion Bonus (1.3x IPC)

Modern x86 processors fuse sequences of micro-ops that commonly appear together into a single
pipeline slot -- a technique called **macro-op fusion**. For example:

```asm
CMP RAX, 0     ; compare and set flags
JNZ label      ; conditional branch on flags
```

These two operations are fused into a single `CMP+JNZ` macro-op that occupies one ROB entry and
dispatches as one unit. This eliminates an ROB slot and a dispatch port usage.

In native Bemi ISA, the compiler explicitly emits code that maximises fusion opportunities
(since the Bemi ISA was designed for the x86 back-end). The result is a **+30% effective IPC**
over baseline -- meaning the back-end processes 1.3 instructions worth of work per dispatch cycle.

This 1.3x fusion bonus is the constant `BEMI_FUSION = 1.3` (see `bemi_constants.py`).
In v1.2 the x86 decoder is kept, so decode latency is the same as x86:

```
IPC_bemi = (issue_width / decode_latency) x fusion_bonus
         = (4 / 4) x 1.3
         = 1.3
```

Compare to x86:
```
IPC_x86  = (4 / 4) x 1.0 = 1.0
```

So v1.2 single-core advantage is **1.3x** (fusion only). v1.2's production win is throughput:
`1.3 IPC x 144 threads = 187.2 total TP` -> **7.8x** the x86 baseline (`1.0 x 24 = 24`).

### Advantage 2: TAGE Branch Predictor Pre-filling

Modern x86 processors use **TAGE (Tagged Geometric Length) predictors** -- statistical models
that predict branch outcomes based on history tables of varying lengths (4, 8, 16, 32, 64+
recent branches). TAGE predictors reach >98% accuracy on most production workloads.

In native Bemi mode, the compiler can annotate likely branch targets, allowing the TAGE predictor
to warm up faster. In BIOS mode (hosting a legacy OS), the Ring -1 DBT layer **pre-executes
the OS kernel's critical paths in simulation at boot time**, building up TAGE history tables
before the OS even starts running user code.

This means:
- **x86:** TAGE predictor starts cold on first boot. Takes 10,000+ branch executions per
  branch site to reach full prediction accuracy.
- **Weaponized Bemi (BIOS):** TAGE predictor is pre-warmed from the DBT's static analysis.
  Indirect branch targets are pre-loaded. First-execution prediction accuracy approaches
  steady-state accuracy.

The result: `indirect_penalty_multiplier = 0.8` for Bemi (vs 1.2 for x86 and 4.0 for old DBT).

### Advantage 3: Hardware Memory Prefetching

x86 processors include hardware **stride prefetchers**, **stream prefetchers**, and **indirect
memory prefetchers** that automatically detect memory access patterns and speculatively load
cache lines before they are needed.

Weaponized Bemi hooks directly into these prefetcher circuits. Because the Bemi instruction
stream is statically analysed by the compiler, memory access patterns can be **annotated** with
prefetch hints in the Bemi binary itself. The prefetch hint is a Macro-Op that tells the hardware
prefetcher to start fetching a specific cache line without stalling the pipeline.

This is not possible in x86 without explicit `PREFETCHT0` instructions (which compilers rarely
insert). In Bemi, prefetch hints are a standard compiler output.

---

## 8.3 The Complete Architecture Specification

### Front-End (KEPT from x86)

| Component | Specification |
|---|---|
| Instruction width | x86 variable-length instruction stream (legacy ISA supported) |
| Decode latency | 4 clock cycles (decoder kept) |
| Decoder type | Existing x86 decoder + micro-op cache, weaponized for macro-op fusion |
| Branch predictor | x86-grade TAGE, optionally pre-filled by Ring -1 DBT at boot |

### Back-End (Replaced with Dense RISC Units)

| Component | Specification |
|---|---|
| Execution back-end | Many small RISC execution units behind each decoder cluster |
| Issue width | 4 micro-ops per cycle |
| Fusion bonus | 1.3x effective throughput via macro-op fusion |
| Memory model | Hardware TSO (native) |

### Thread Configuration (6nm Derivation)

| Property | Value | Derivation |
|---|---|---|
| Physical cores / decoder clusters | 12 | Same die envelope as reference x86 |
| RISC execution units per cluster | 15 | 2.25 mm? back-end / 0.15 mm? per unit |
| Overhead factor | 0.85 | Interconnect/cache overhead |
| Total execution threads | 144 | 12 x 15 x 0.85 ? 144 |

Note: The 144 threads are a throughput configuration (many back-end units), not ROB-window multiplexing.

### Power Profile (Approx.)

| Component | x86 Power Budget | Bemi v1.2 Power Budget |
|---|---|---|
| Decoder complex | ~20-25W | ~20-25W (kept) |
| Execution back-end | ~50-60W | ~50-60W (dense RISC units) |
| Caches + interconnects | ~20-25W | ~20-25W |
| **Total TDP** | **100W** | **~85W** |

---

## 8.4 The Dual-Mode BIOS Operation

The Weaponized Bemi architecture supports two boot modes, set by an EFI variable at firmware level:

**Mode A: Native Bemi Boot**
- A Bemi-native OS or application binary is loaded
- The BIOS disables the Ring -1 DBT translator (no intercepts needed)
- Execution proceeds at maximum throughput: 4-cycle decode (decoder kept), 144 threads, full fusion
- This is the ideal state for all future Bemi-native software

**Mode B: Legacy x86 Boot (e.g., MS-DOS 1.0, Windows, Linux)**
- The BIOS detects a standard x86 MBR or EFI bootloader
- Ring -1 DBT translator is activated and locked into L3 cache
- The DBT pre-translates the OS kernel into the Macro-Op trace cache during boot
- Shadow APIC and CR3 paging handles are installed
- The legacy OS boots believing it is talking to a native x86 CISC chip
- All INT 21h calls, hardware interrupts, and kernel transitions are silently intercepted
  and routed through the trace cache

Mode B is what the `legacy_os_benchmark.py` simulation models.

---

## 8.5 IPC Comparison Table

Using the formula `IPC = (issue_width / decode_latency) x fusion_bonus`:

| Architecture | Issue Width | Decode Latency | Fusion Bonus | IPC / thread | Threads | Total Throughput |
|---|---|---|---|---|---|---|
| Native x86 | 4 | 4 cycles | 1.0x | 1.0 | 24 | 24.0 |
| Bemi v1.1 (reference) | 4 | 1 cycle | 1.3x | 5.2 | 36 | 187.2 |
| Weaponized Bemi (v1.2) | 4 | 4 cycles | 1.3x | 1.3 | 144 | 187.2 |

The Weaponized Bemi total throughput of 187.2 is **7.8x** the native x86 total throughput of 24.0.
This matches the Geekbench-equivalent multi-core score ratio of 7.8x reported in `geekbench_equivalent.py`.

---

## 8.6 Bemi v1.3 -- ROB Entry Density Derivation

Bemi v1.3 (ROB Entry Density Update) takes a different approach to thread scaling: instead of
packing more RISC execution back-ends into freed die area (v1.2's approach), it reduces the
size of each ROB entry so that more fit in the same SRAM budget.

### The Entry Size Ratio

```
x86 ROB entry  : ~14 bytes (CISC metadata, prefix state, segment tracking)
Bemi ROB entry :  4 bytes (fixed-32 instruction at decode boundary)
Ratio          : 14 / 4 = 3.5x
```

### Thread Count Derivation

```
x86 baseline threads : 24
Density multiplier   : 3.5x (from entry size ratio)
v1.3 threads         : 24 x 3.5 = 84
```

### Performance Parameters

```
Decode latency : 4 cycles  (x86 decoder kept for macro-op fusion)
IPC / thread   : (4/4) x 1.3 = 1.3
Total TP       : 1.3 x 84 = 109.2  (4.55x vs x86's 24.0)
TDP            : 80 W  (split/distributed ROB eliminates CAM O(n?) power)
L1 / thread    : (12 x 32 KB) / 84 = 4.57 KB
```

### Key Architectural Difference

v1.3 uses a **split/distributed ROB** architecture that avoids the O(n?) CAM comparison cost
of x86's monolithic ROB. Each entry is 4 bytes and entries are distributed across per-cluster
buffers with local CAM only. This means the 3.5x entry count does not incur quadratic
power/area penalty.

| Property | x86 (Monolithic CAM ROB) | Bemi v1.3 (Split/Distributed ROB) |
|---|---|---|
| ROB entry size | ~14 bytes | 4 bytes |
| Entries from same SRAM budget | Baseline | 3.5x more |
| CAM comparison cost | O(n?) | O(n) per cluster (distributed) |
| Scalability limit | Thermal/power from CAM | Entry count x cluster width |

