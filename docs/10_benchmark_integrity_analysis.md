# 10. Benchmark Integrity Analysis -- What Was Wrong

## 10.1 The Audit Mandate

Before the final honest benchmark suite was constructed, the existing benchmark codebase underwent
a systematic integrity audit. The purpose: identify every instance where the simulation was
structured to guarantee a Bemi win, rather than derive it from physics-grounded models.

The audit found **five major categories of cheating** and several minor oversights.
This chapter documents each one precisely -- what the code did, what it should have done,
and why the distinction matters.

---

## 10.2 Cheat 1: The Magic IPC Multiplier (`legacy_os_benchmark.py`)

### What the code did

```python
x86_baseline = OSEnvironment(
    name="Legacy BIOS + Native x86 CPU",
    syscall_cost=150,
    backend_ipc=1.0      # x86 gets IPC = 1.0
)

bemi_legacy = OSEnvironment(
    name="BEMI BIOS + DBT Firmware + Legacy x86 OS",
    syscall_cost=160,    # slight "penalty" (actually negligible)
    backend_ipc=3.8      # Bemi magically gets 3.8x IPC
)

exec_time = (sys_overhead + int_overhead) / self.backend_ipc
```

Bemi was declared 3.8x faster by inserting `backend_ipc=3.8` directly into the denominator.
No mechanism was modelled. There was no simulation of trace-cache hits, of 1-cycle decode,
of thread density, or of anything physical. The benchmark was simply an equation where the
result was hardcoded by the constant chosen.

### Why it was wrong

The `backend_ipc` parameter has no physical definition. It is a label placed on a multiplier
that produces the desired output. The "slight intercept penalty" (160 vs 150 syscall cost)
was visible but completely swamped by the 3.8x multiplier -- by design.

### The Fix

Replace `backend_ipc` with a **derived throughput formula**:

```python
throughput = (threads / (decode_latency + tso_penalty)) * ipc_fusion
```

Where every component has a concrete physical meaning:
- `threads`: either 24 (x86) or 36 (Bemi) -- from hardware spec
- `decode_latency`: 4 cycles (x86 CISC) or 1 cycle (Bemi fixed-32) -- from micro-op timing tables
- `tso_penalty`: 0 for both (hardware TSO in both cases) -- from architecture docs
- `ipc_fusion`: 1.0 (x86) or 1.3 (Bemi, from `optimized_x86_bemi_bench.py`) -- documented

The speedup is now **emergent from the physics**, not injected as a constant.

---

## 10.3 Cheat 2: Zero-Cycle Decode in the VM Simulation (`bemi_vm.py`)

### What the code did

```python
def _bemi_worker(instruction_cycles):
    # "1-cycle instantaneous decode. Zero stall."
    time.sleep(0.000)          # <- literally 0 seconds simulated
    time.sleep(instruction_cycles * 0.001)
    return True
```

The comment said "1-cycle decode" but the implementation used `sleep(0.000)` -- zero time.
Meanwhile the x86 worker used `sleep(0.004)` (4 cycles).

### Why it was wrong

A 1-cycle decode still takes 1 clock cycle. The fixed-32 decoder reads 4 bytes and produces
a routing signal -- it is not free. Using `sleep(0.000)` simulated a decoder that does not
exist -- an instruction stream that appears from thin air with no processing time.

Additionally, even though the code claimed Bemi used 8x instruction expansion (800 instructions
vs x86's 100), the thread counts were deliberately skewed: x86 got 12 threads, Bemi got 36.
The correct counts are 24 and 36. The extra 12 threads for Bemi (vs x86's deficit of 12) were
a hidden thread-count cheat that compounded the zero-decode cheat.

### The Fix

```python
def _bemi_worker(instruction_cycles):
    time.sleep(0.001)          # 1 cycle -- fast but not free
    time.sleep(instruction_cycles * 0.001)
    return True
```

And thread counts corrected: x86 = 24, Bemi = 36.

---

## 10.4 Cheat 3: Impossible Cycle Counts in final_benchmarks.py

### What the code did

```python
x86_cycles  = [5, 8, 8, 6]   # x86 cycles per workload
bemi_cycles = [1, 4, 4, 2]   # Bemi cycles per workload
```

For AVX-512: Bemi got **4 cycles total** while x86 got 8 cycles. This implies Bemi's
AVX-512 execution takes only 4 cycles -- which is the execute time of the 512-bit FMA unit itself.

But where is the decode? Even with a 1-cycle fixed decoder, the decode still happens. The
`bemi_cycles[1] = 4` implied a 0-cycle decode on a passthrough workload, not a 1-cycle decode.

More critically: for General Integer Math, Bemi got **1 cycle total** (1 decode + 0 execute?).
This is physically impossible -- every instruction needs at least 1 execute cycle.

### Why it was wrong

The benchmark simultaneously:
1. Gave Bemi 0-cycle decode (impossible)
2. Gave Bemi fewer execute cycles than x86 on the same hardware (impossible -- they use the same ASIC)
3. Gave Bemi a lower TDP (45W instead of 65W)
4. Used wrong thread counts (still 12 vs 36 in the x86/Bemi label text)

Each error independently favoured Bemi. Together they stacked into a massive fabricated advantage.

### The Fix

```python
# Correct cycle model: x86 = decode + execute, Bemi = 1 decode + same execute
x86_cycles  = [5, 8, 8, 6]   # 4 decode + N execute (unchanged)
bemi_cycles = [2, 5, 5, 3]   # 1 decode + N execute (execute cycles IDENTICAL -- same ASIC)
```

Thread counts fixed to 24 vs 36. TDP corrected to 65W. All speedups now emergent.

---

## 10.5 Cheat 4: Thread Count Fabrication

### What the code did

Almost every benchmark file defined thread counts as:
```python
x86  = Architecture("Native x86",   cores=12, threads_per_core=2)  # 24 threads
bemi = Architecture("Bemi RISC",    cores=36, threads_per_core=4)  # 144 threads (!)
```

Bemi was given 36 physical cores **and** 4 threads per core = **144 threads**.
x86 was given 12 physical cores and 2 threads per core = 24 threads.

This is a **6x thread count advantage** handed to Bemi by fiat.

### Why it was wrong

The Bemi architecture does not have 36 physical cores. It has **12 physical cores** -- the same
silicon area as the reference x86. The 3x density multiplier refers to **3x virtual thread
extraction via ROB density** -- not 3x more physical cores.
(The v1.3 ROB Entry Density model extends this to 3.5x by comparing entry sizes: x86 14B vs Bemi 4B.)

A processor with 36 physical cores would require approximately 3x the die area, 3x the power,
and cost 3x as much. Comparing a 36-core chip to a 12-core chip and calling it a fair architectural
comparison is nonsensical.

The correct comparison is:
- 12 physical cores, 2-way SMT = **24 threads** (x86)
- 12 physical cores, 3x ROB virtual extraction = **36 virtual threads** (Bemi)

This gives Bemi a genuine **1.5x thread advantage**, not 6x.

### The Fix

Thread counts corrected globally across all 13 benchmark files to 24 vs 36.

---

## 10.6 Cheat 5: RISC Instruction Expansion Ignored (`arithmetic_memory.py`)

### What the code did

```python
ops, mem_ops = 10**9, 10**7   # 1 billion operations

# x86 and Bemi given IDENTICAL operation counts
# Bemi RISC executed the same 10^9 operations as x86 CISC
```

### Why it was wrong

A RISC architecture requires **more instructions** to accomplish the same high-level work than
CISC. This is the fundamental trade-off of RISC. An `ADD RAX, [RCX]` CISC instruction becomes
`LOAD RTmp, [RCX]` + `ADD RAX, RAX, RTmp` -- 2 instructions instead of 1.

By simulating the same operation count for both architectures, the benchmark ignored the single
most important cost of RISC. This is equivalent to benchmarking a 1-speed bicycle against a
10-speed bicycle and giving both the same pedalling effort.

### The Fix

Applied realistic expansion factors:
- **Arithmetic operations:** 1.5x expansion (CISC `ADD [mem], reg` -> RISC Load + ADD + Store)
- **String operations:** 8x expansion (CISC `REP MOVSB` -> RISC loop with Load, Store, Inc, Cmp, Branch)
- **SIMD (with passthrough):** 1.0x expansion (Macro-Op translates 1:1)

```python
bemi_ops = ops * self.instr_expansion   # 1.5x for arithmetic, 8x for strings
```

---

## 10.7 Summary of All Fixes Applied

| Issue | File(s) | Old Value | Corrected Value |
|---|---|---|---|
| Hardcoded IPC multiplier | legacy_os_benchmark.py | backend_ipc=3.8 | Derived from formula |
| Zero-cycle decode | bemi_vm.py | sleep(0.000) | sleep(0.001) |
| Impossible Bemi cycle counts | final_benchmarks.py | [1, 4, 4, 2] | [2, 5, 5, 3] |
| Bemi thread count (physical cores) | All 13 files | 36 cores x 4 = 144 | 12 cores x 3 virt = 36 |
| x86 thread count | All 13 files | 12 (inconsistent) | 24 consistently |
| RISC expansion ignored | arithmetic_memory.py | 1.0x (no expansion) | 1.5x arithmetic, 8x strings |
| TDP (Bemi) | final_benchmarks.py, power_efficiency.py | 45W | 65W |
| AI training throughput | ai_training.py | Magic constants | IPC model formula |
| Geekbench scores | geekbench_equivalent.py | Magic base/mod values | IPC model formula |
| Memory cache sizing | memory_hierarchy_bench.py | Free larger caches for Bemi | Same physical pool, shared |
| TSO contention scaling | tso_concurrency_bench.py | Scaled with virtual threads | Scaled with physical cores |

