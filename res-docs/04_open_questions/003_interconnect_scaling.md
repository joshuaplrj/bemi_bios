# Open Question -- Interconnect Scaling for 144 Execution Units

**Filed:** 2024-11-08
**Priority:** High -- may limit v1.2's theoretical thread count

---

## The Problem

Weaponized Bemi v1.2 claims 144 virtual threads from 12 decoder clusters x 15 RISC execution units per cluster. But how do 15 execution units share a single L1 data cache?

## The Numbers

Assume each RISC execution unit has:
- 1 integer ALU
- 1 FP/SIMU ALU (128-bit)
- 1 load/store unit
- A 32-entry physical register file

That's 15 load/store units per decoder cluster, all needing access to the L1 data cache.

**L1D cache characteristics (typical):**
- Size: 32 KB
- Associativity: 8-way
- Banks: 2 or 4 (for concurrent access)
- Ports: 2-3 read + 2 write per cycle (with banking)

**Problem:** 15 load/store units x 1 access/cycle = 15 accesses/cycle, but L1D can handle only 4-5 accesses/cycle (with 2-3 read ports and banking).

## Options

### Option 1: Port Multiplication

Increase L1D ports from 3 to 15. This is infeasible -- each additional port adds O(n?) area to the SRAM array. A 15-port L1 cache would be ~25x larger than a 3-port cache.

### Option 2: Banking

Split the L1 into 15 independently addressable banks. Each bank is a standard 2-port SRAM. A crossbar routes load/store unit addresses to banks.

**Crossbar cost:**
- 15 input x 15 output crossbar: 225 crosspoints
- Each crosspoint: ~50 transistors -> 11,250 transistors (negligible)
- Wire delay dominates: 15x15 crossbar at 6nm has ~200 ps delay (within 1 cycle at 5 GHz)

**Bank conflict probability:**
- 15 random accesses across 15 banks: expected conflicts = 15 - 15*(14/15)^14 ? 8.5 conflicts/cycle
- With perfect hashing: ~57% of accesses conflict

**Effective bandwidth:** ~6.5 accesses/cycle (less than the 15 we need)

### Option 3: Clustering with Private L0 Caches

Give each RISC execution unit a tiny private L0 cache (1-2 KB, 1-cycle latency, 2 ports). L0 hit rate for well-tuned workloads: ~70-80%.

With L0 cache:
- 70% hit on L0: no L1 access needed
- 30% miss -> 15 x 0.3 = 4.5 L1 accesses/cycle
- This is achievable with 6 L1 ports or 8 banked L1 slices

**Trade-off:** L0 cache adds ~0.05 mm? per unit (15 x 0.05 = 0.75 mm? per cluster = 9 mm? total for 12 clusters). This is a 20% increase in back-end area, reducing unit count from 15 to ~12 per cluster. Thread count drops from 144 to ~122.

## Preliminary Assessment

Interconnect is a real constraint but not a showstopper. The L0 cache clustering approach (Option 3) seems viable with modest thread reduction (122 vs 144).

## Open Issues

1. **Cache coherence between L0 caches:** Do L0 caches need to snoop each other? If the 15 units run threads sharing data, yes. This requires a snoop filter, adding area and latency.
2. **Non-uniform memory access:** If L0 caches create NUMA effects within a cluster, thread scheduling becomes complex. The compiler must be NUMA-aware.
3. **Energy:** L0 cache hits are 5x cheaper than L1 hits. But the L0 miss rate compounds: 30% of accesses go to L1, then some fraction of L1 misses go to L2/L3. The energy per instruction could be higher than a conventional x86 core despite the smaller back-end units.

## Next Steps

Build a cycle-accurate simulation of a 15-unit cluster with L0 caches and banked L1. Key output: achievable IPC per unit under memory-intensive vs compute-intensive workloads.

Current estimate: **~100-110 effective threads** after interconnect and L0 overhead, vs the claimed 144. This is still 4.2-4.6x the x86 baseline of 24 threads.

