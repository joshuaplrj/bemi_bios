# 01. The x86 Problem & The Bemi Hypothesis

## 1.1 The Fundamental Problem with x86

Modern x86 processors are engineering marvels, but they carry a structural burden from 1978 that
every generation of silicon has had to work around: **variable-length instruction encoding**.

In the x86 ISA, a single instruction can be anywhere from **1 to 15 bytes long**. The CPU has no
prior knowledge of where one instruction ends and the next begins when it receives a raw byte stream
from memory. Before anything useful can happen -- before the CPU can decode, schedule, or execute
anything -- it must first solve a non-trivial parsing problem on every instruction it receives.

### The x86 Front-End Penalty

The x86 front-end performs the following mandatory steps for every instruction:

1. **Fetch**: Pull up to 16 bytes from the L1 instruction cache into the fetch buffer.
2. **Pre-decode**: Scan byte-by-byte to locate instruction boundaries. The CPU cannot assume where
   one instruction ends. It must examine prefix bytes, REX bytes, opcode bytes, and ModRM bytes
   in sequence to calculate the total instruction length.
3. **Decode**: Route the instruction to one of three hardware paths:
   - **Simple decoder**: For 1-micro-op instructions (e.g., `ADD RAX, RBX`).
   - **Complex decoder**: For instructions that crack into 2-4 micro-ops.
   - **Microcode Sequencer (MSROM)**: For microcoded instructions (e.g., `REP MOVSB`, `CPUID`,
     `XSAVE`) that expand into large sequences of internal micro-ops.
4. **Micro-op Cache (L0)**: Decoded micro-ops are cached to avoid re-decoding on hot loops.

This front-end machinery consumes an estimated **20-30% of the total die area** on modern x86
processors. It burns power, generates heat, and adds a **4-cycle stall latency** to every
instruction that misses the micro-op cache.

### The Decoder Tax: A Constant Background Drain

Even when the micro-op cache is warm, the decoder complex must remain powered to handle cache
misses, self-modifying code, and cold start paths. This creates a floor of wasted power and silicon
that cannot be eliminated within the x86 ISA contract.

> **Key insight:** x86's variable-length encoding is not a bug -- it is a deliberate design choice
> that maximises code density. A program compiled for x86 is smaller in bytes than the same program
> compiled for RISC. The trade-off is that the *CPU* pays the parsing cost on every execution,
> instead of the *compiler* paying it once at compile time.

---

## 1.2 The Bemi Hypothesis

Project Bemi begins with a single question:

> *What happens to a processor's performance and power budget if you physically remove the x86
> decoder and replace it with a fixed-length 32-bit RISC decoder?*

The hypothesis has three parts:

### Part A: Decode Latency Collapses

A fixed-length instruction is exactly 4 bytes wide. The decoder reads 4 bytes and immediately
knows it has one complete instruction. There is no boundary scanning, no prefix accumulation, no
opcode disambiguation. The decode latency collapses from **4 cycles to 1 cycle**.

### Part B: Reclaimed Silicon Enables 3x Thread Density

The silicon area freed by removing the x86 decoder complex (~20-30% of die) can be repurposed.
Bemi's thesis is that this area is best used to triple the number of execution threads and
the depth of the Reorder Buffer (ROB).

The specific claim: **where a standard x86 die supports 12 physical cores (24 threads via SMT),
a same-area Bemi die can support 12 physical cores with 3x virtual thread extraction (36 threads)
through ROB density.**

This is not magic -- it is an engineering trade: decode complexity traded for execution width.

### Part C: The Parallelism Gain Outweighs Instruction Expansion

RISC instructions are simpler. A single CISC instruction like `ADD RAX, [RCX]` (add a value from
memory to a register) becomes two RISC instructions: `LOAD RTmp, [RCX]` and `ADD RAX, RAX, RTmp`.
This **instruction expansion** is a real cost.

The Bemi hypothesis is that the 3x thread density gain mathematically overwhelms the expansion
penalty for most workloads, because more threads means more instructions can execute in parallel.

---

## 1.3 The Mathematical Framework

Let us define the comparison formally.

For a given workload with `W` units of high-level work:

**x86 execution time:**
```
T_x86 = (W * expansion_x86 * (decode_x86 + exec_cyc)) / threads_x86
       = (W * 1.0 * (4 + exec_cyc)) / 24
```

**Bemi execution time:**
```
T_bemi = (W * expansion_bemi * (decode_bemi + exec_cyc)) / threads_bemi
        = (W * expansion_bemi * (1 + exec_cyc)) / 36
```

**Bemi wins when `T_bemi < T_x86`**, i.e.:**

```
(expansion_bemi * (1 + exec_cyc)) / 36 < (1.0 * (4 + exec_cyc)) / 24
```

Simplifying:

```
expansion_bemi < (36 / 24) * (4 + exec_cyc) / (1 + exec_cyc)
expansion_bemi < 1.5 * (4 + exec_cyc) / (1 + exec_cyc)
```

For a pure arithmetic workload (`exec_cyc = 1`):
```
expansion_bemi < 1.5 * (5 / 2) = 3.75
```

So Bemi wins on arithmetic as long as the RISC instruction expansion is less than **3.75x**.
The observed expansion for arithmetic is **1.5x** (a CISC `ADD [mem], reg` becomes Load + ADD + Store).
This means Bemi wins by a large margin on arithmetic workloads -- which is exactly what the benchmarks confirm.

---

## 1.4 The Original Verification Roadmap

The original `idea.md` roadmap defined three verification phases:

**Phase 1 -- Native x86 Baseline**: Establish reference measurements for instruction throughput,
power draw, and thread utilisation on unmodified x86 hardware.

**Phase 2 -- Bemi RISC Execution**: Run identical workloads through the Bemi compiler/translator.
Measure IPC and effective FLOPS with 36-thread virtual density.

**Phase 3 -- Head-to-Head**: Quantify performance deltas across single-threaded vs. multi-threaded
workloads. Validate the "Apple-style performance leap" claim on existing x86 silicon.

This three-phase plan became the backbone of all subsequent benchmarking work, culminating
in the `bemi_bios/run_all_benchmarks.py` suite documented in Chapter 12.

