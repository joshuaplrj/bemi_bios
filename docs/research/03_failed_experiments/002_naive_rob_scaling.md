# Failed Experiment -- Naive ROB Scaling (v1.0)

**Status:** ABANDONED -- 2024-11-02
**Duration investigated:** 2 weeks
**Published doc reference:** `docs/07_native_isa_evolution.md` (v1.1 is the successor)

---

## What We Tried

The original BEMI v1.0 concept was deceptively simple: **remove the x86 decoder and scale the ROB depth by the area saved.** If the decoder takes ~25% of the die, and the ROB takes ~10%, then freeing decoder area lets us scale the ROB by `(25+10)/10 = 3.5x`.

Naively: 3.5x deeper ROB -> 3.5x more in-flight instructions -> 3.5x thread density.

## Why It Failed

### Problem 1: ROB Scaling Isn't Linear

The ROB (Reorder Buffer) is not just a FIFO -- it's a fully-associative content-addressable memory for dependency tracking. Doubling ROB depth does **not** double capacity:

| ROB Depth | Area | Access Latency | Power |
|---|---|---|---|
| 100 entries | 1x | 1 cycle | 1x |
| 200 entries | 2.5x | 1.3 cycles | 2.2x |
| 300 entries | 5x | 1.7 cycles | 4x |
| 400 entries | 9x | 2.2 cycles | 7x |

ROB area scales **quadratically** with depth (CAM arrays have O(n?) bitline and matchline capacitance). The 3.5x depth increase we assumed would require ~12x the ROB area -- far exceeding our 2.5x area budget.

**Root cause:** We assumed O(n) ROB scaling. Physical CAM design is O(n?). This was a freshman-level error in VLSI design.

### Problem 2: ROB Depth vs Thread Count Mismatch

Thread density is not determined by ROB depth alone. Even with 3x deeper ROB, you need:

1. **Register renaming capacity** -- more physical registers to support more in-flight instructions. The Physical Register File (PRF) scales similarly to ROB (O(n?) for read ports).
2. **Load/Store Queue capacity** -- more in-flight memory operations need larger LSQ. LSQ is a CAM too.
3. **Scheduler wakeup logic** -- more entries means more wakeup buses, more tag broadcast, more power.

Doubling ROB without matching these structures creates a bottleneck elsewhere. The net gain is closer to **1.5x** than 3.5x.

### Problem 3: The Critical Path Gets Worse

The ROB's wakeup logic is on the critical path of the CPU pipeline. A deeper ROB means:
- Longer tag broadcast buses (1.5x deeper = 1.5x longer wires = slower wakeup)
- More comparison logic (O(n?) in number of wakeup ports)
- The cycle time may need to increase to accommodate the deeper ROB

If cycle time increases by 10%, the 3.5x ROB depth only gives 3.2x effective throughput -- and that's before the O(n?) area penalties.

## The Numbers

| Claim | v1.0 Assumption | Corrected Value | Error |
|---|---|---|---|
| Decoder area freed | 25% of die | 25% | Correct |
| ROB area as % of die | 10% | 12% | Minor |
| ROB depth multiplier | 3.5x | 1.8x | -49% |
| Effective thread gain | 3.5x | 1.5x | -57% |
| Power increase | 1x (neutral) | 1.4x | +40% |
| Cycle time impact | None | +8% | Added |

## What Replaced It

The naive ROB scaling model was replaced by two successive architectures:

1. **v1.1** (Chapter 7): Still removes the decoder, but uses the area for **wider issue** (16-wide) plus moderate ROB scaling. 5.2x IPC per thread, 36 threads. This is more realistic than v1.0 because it acknowledges the O(n?) scaling limits of CAM structures.

2. **v1.2 / Weaponized Bemi** (Chapter 8): Keeps the decoder, replaces the execution back-end with many small RISC units. This side-steps the ROB scaling problem entirely -- thread density comes from physical back-end replication, not ROB depth.

## Key Lesson

**Never assume O(n) scaling for CAM-based structures in a CPU.** ROB, PRF, LSQ, and scheduler all scale O(n?) or worse. The Weaponized v1.2 approach of replicating small back-end units is architecturally superior because it avoids deep CAM arrays entirely.

## Residual Risk

The v1.2 model introduces its own scaling challenge: interconnect. 144 execution units need a crossbar to access the L1 cache. Crossbars scale O(n?) in area and O(log n) in latency. This is acknowledged but not yet analyzed -- see `04_open_questions/003_interconnect_scaling.md`.

