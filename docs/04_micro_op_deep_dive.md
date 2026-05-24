# 04. The Micro-Op Deep Dive

## 4.1 Why Micro-Op Size Matters

Bemi's entire argument rests on the idea that fixed-length 32-bit instructions are better than
variable-length x86 instructions. But what about the *internal* micro-ops that x86 processors
generate? Are they fixed-length? Are they better or worse than Bemi's format?

This chapter answers those questions in detail, drawing from `micro-ops.md` -- a technical
analysis of x86 micro-op anatomy derived from patent analysis, die shots, and microcode research.

The key finding: **x86 micro-ops are neither fixed-length nor small.** They range from 50 bits
(in the compressed L0 cache) to 110+ bits (in the ROB scheduler). Bemi's 32-bit format is
genuinely more compact and efficiently cacheable.

---

## 4.2 What is a Micro-Op (?op)?

When an x86 CPU decodes an instruction, it converts it into one or more **micro-ops** -- internal
primitive operations that the out-of-order execution engine can schedule, rename, and dispatch
independently.

**The critical axiom:** Zero x86 macro-instructions execute "natively" in the execution engine.
Every single x86 instruction -- even `ADD RAX, RBX` -- is decoded into at least one ?op before
it can be executed. The routing is deterministic, hardware-coded based on opcode and addressing mode.

The three decode paths:

| Path | Throughput | ?op count | Examples |
|---|---|---|---|
| Simple Decoders | 4 instructions/cycle | 1 ?op each | `ADD r,r`, `MOV r,r`, `JMP rel` |
| Complex Decoders | 1 instruction/cycle | 2-4 ?ops | `ADD r,[mem]`, complex addressing |
| Microcode Sequencer | Varies | >4 ?ops (from ROM) | `REP MOVSB`, `CPUID`, `XSAVE` |

---

## 4.3 The Anatomy of a ?op Payload

A fully-expanded ?op must carry all the information the execution engine needs to schedule and
execute it out-of-order. The mathematical model for total ?op width is:

```
W_total = W_opcode + sum(W_sources) + W_destination + W_immediate + W_control
```

Breaking this down with approximate real-world bit widths:

| Field | Size | Purpose |
|---|---|---|
| Micro-Opcode | 8-12 bits | The internal hardware operation (INT_ADD, FP_MUL_256, MEM_LOAD) |
| Source 1 Tag | 8-10 bits | Physical register file (PRF) pointer after register renaming |
| Source 2 Tag | 8-10 bits | Second source PRF pointer |
| Destination Tag | 8-10 bits | Output PRF pointer |
| Execution Port | 4-6 bits | Which ALU/execution port can accept this op |
| Immediate/Displacement | 32-64 bits | Constant values or memory offsets |
| Control/Status Flags | 10-15 bits | Exception flags, memory ordering tags, branch validation bits |

**Total: 70 to 120+ bits per ?op in the execution engine.**

This is not a fixed size -- it expands as the ?op flows through the pipeline.

---

## 4.4 The Pipeline Expansion: ?op Size Changes in Flight

A ?op does not maintain a constant size throughout the CPU pipeline:

### Phase A: The L0 ?op Cache (Compressed Format)

Modern CPUs cache decoded ?ops to avoid re-decoding on hot loops. Inside the L0 ?op cache
(also called the "Decoded Instruction Cache" or DIDC), ?ops are stored in a compressed format
to save power: approximately **50 to 64 bits per ?op**.

At this stage, physical register tags haven't been assigned yet (that happens at rename).
The ?op contains only the logical opcode and logical register references.

### Phase B: Rename & ROB Allocation

As the ?op moves from the front-end to the back-end, it is allocated into the **Reorder Buffer
(ROB)** and the **Reservation Stations**. At this point:

1. **Register renaming** assigns physical register file (PRF) tags. This eliminates false
   Write-After-Read (WAR) and Write-After-Write (WAW) data hazards.
2. **Scheduler dependency tracking** marks which PRF slots the ?op must wait for before it
   can dispatch to an execution unit.

After renaming, the ?op has swelled to its maximum width -- often **110+ bits** in the scheduler.
Intel's monolithic ROB structure (as opposed to ARM's split structure) must track every in-flight
instruction across all execution units simultaneously, which drives this bloat.

### Phase C: Execute and Retire

Once the ?op executes in the ALU, it sheds the scheduling metadata. The only thing that
persists into the retirement phase is the result value and the physical register tag to write it to.
The ?op then releases its ROB entry and physical register file slots for reuse.

---

## 4.5 AVX-512 Special Case: The 512-Bit Vector ?op

A critical misconception must be addressed: **a ?op for a 512-bit AVX instruction does NOT contain
512 bits of data.**

When `VADDPS zmm1, zmm2, zmm3` (add 16 single-precision floats) is decoded, the resulting ?op
is still only ~100 bits wide. It contains **pointers** (8-10 bit physical register tags) that
tell the 512-bit Vector ALU where to fetch the actual 512-bit data from the Vector Physical
Register File exactly one clock cycle before execution.

The 512-bit data lives in the Vector PRF, not in the ?op itself. The ?op is just the scheduling token.

This has an important implication for Bemi: when the Macro-Op Passthrough routes a Bemi Macro-Op
to the 512-bit FMA unit, the Macro-Op itself is 32 bits wide and carries only register index
references. The actual 512-bit data is fetched from the same physical vector register file that
the host x86 system uses. **No data format conversion is required.**

---

## 4.6 Comparing x86 ?op Format vs Bemi Fixed-32 Format

| Property | x86 ?op | Bemi Fixed-32 |
|---|---|---|
| Size (L0 cache) | 50-64 bits | 32 bits |
| Size (ROB scheduler) | 110+ bits | 32 bits (decode size) |
| Length | Variable within pipeline stages | Always 32 bits |
| Decoder to produce | Complex hardware, 4-cycle stall | Trivial, 1-cycle |
| L1 instruction cache density | Lower (fewer ops per cache line) | Higher |
| Out-of-order capability | Full (PRF renaming) | Full (PRF renaming) |

The Bemi 32-bit format is **smaller and simpler** at the decode boundary. Once the instruction
enters the ROB and gets renamed, both architectures work identically -- the back-end is shared
(Bemi's Weaponized mode literally uses the same back-end silicon).

---

## 4.7 The Monolithic ROB vs Bemi's ROB Density

Intel and AMD use a **monolithic ROB** -- a single large unified buffer that tracks every in-flight
?op across all execution units. ARM's M-series processors use a **split ROB** structure with
smaller per-cluster buffers, which reduces die area.

Bemi's thread density multiplier comes from choosing the right ROB approach for fixed-32 instructions:

1. **Fixed-32 decode** means the ROB entry size at the front-end boundary is known at design time.
   This allows the ROB to be sized optimally without padding for variable-length decode artifacts.
2. **x86 ROB entries are ~14 bytes** -- carrying CISC metadata, prefix state, and segment tracking.
   **Bemi ROB entries are 4 bytes** -- the 32-bit instruction itself at the decode boundary.
3. **The same SRAM budget holds 3.5x more Bemi entries** (14 / 4 = 3.5), giving v1.3 its
   84-thread count (24 x 3.5) from the same SRAM that x86 allocates to only 24 threads.
4. **Bemi's split/distributed ROB** avoids the O(n?) CAM comparison penalty of a monolithic
   structure, so entry density scales without quadratic power growth.

### v1.3 Entry Size Comparison

| Property | x86 (Baseline) | Bemi v1.3 |
|---|---|---|
| ROB entry size | ~14 bytes | 4 bytes |
| SRAM budget (ROB) | X bytes | X bytes (same) |
| Max entries | X/14 | X/4 = 3.5x more |
| Virtual threads | 24 | **84** (24 x 3.5) |
| ROB structure | Monolithic CAM (O(n?)) | Split/distributed (O(n) per cluster) |

The 3.5x multiplier is thus not a claim about having 3.5x more cores. It is a claim about having
**3.5x more concurrently in-flight micro-ops from the same SRAM budget**, which is mathematically
equivalent to 3.5x thread density for throughput-bound workloads.

---

## 4.8 Practical Implication: L1 Instruction Cache Hit Rate

With x86 ?ops in the L0 cache at 50-64 bits each, and Bemi fixed-32 instructions at 32 bits each:

- An L1 cache line is typically 64 bytes = 512 bits
- L0 cache fits: `512 / 57 (avg) ? 9` x86 ?ops per cache line
- L1 fits: `512 / 32 = 16` Bemi instructions per cache line

Bemi packs **~1.78x more instructions per cache line** than x86 ?op streams. This translates to
a meaningfully higher L1 instruction cache hit rate for dense instruction loops -- which is why
`arithmetic_memory.py` shows Bemi maintaining competitive hit rates despite 36 virtual threads
fighting over the same physical L1 pool.

