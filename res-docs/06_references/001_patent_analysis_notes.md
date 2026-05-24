# Reference Notes -- x86 Microarchitecture Patent Analysis

**Date:** 2024-10-05 to 2024-10-18
**Source:** USPTO / Google Patents
**Relevance:** Used to validate ?op pipeline model in `docs/04_micro_op_deep_dive.md` and `docs/05_cisc_dominance_problem.md`

---

## Patents Reviewed

| Patent # | Title | Key Insight |
|---|---|---|
| US 9,880,767 | "Decoded instruction cache with compressed storage" | L0 ?op cache stores 50-64 bit compressed ?ops; decompression adds 1 cycle latency on miss |
| US 10,346,171 | "Micro-op fusion apparatus and method" | CMP+JCC fusion saves 1 ?op per fused pair; fusion limited to specific opcode combinations |
| US 9,116,688 | "Out-of-order execution micro-architecture" | Detailed ROB CAM structure with O(n?) bitline capacitance; confirms our O(n?) scaling model |
| US 10,545,804 | "Physical register file allocation retirement" | PRF uses 8-10 bit physical register tags; 300+ physical registers for 180 architectural registers |
| US 8,880,849 | "Load-store queue with speculative wakeup" | LSQ CAM array: 72 entries x 3 ports = 216 compare operations per cycle |
| US 11,176,619 | "TAGE branch predictor with geometric history lengths" | Confirms geometric history design; 4 tables (T1-T4) with 4-64 branch history lengths |

---

## Key Architectural Details Extracted

### L0 ?op Cache Compression (US 9,880,767)

- Compressed ?op format: 50-64 bits (vs ~100 bits in the ROB)
- Compression works by eliding physical register tags (they're assigned at rename, not at decode)
- L0 cache hit: 0 cycle latency (pipeline bypass)
- L0 cache miss: 4 cycle penalty (re-decode from L1I cache)
- Cache size: 1.5K-2K ?ops per core (varies by microarchitecture)

**BEMI relevance:** BEMI's 32-bit fixed-length instructions are smaller than even the compressed ?op format. This confirms the L1 cache density advantage claimed in Chapter 4.8.

### Macro-Op Fusion Patterns (US 10,346,171)

Only specific instruction pairs can be fused:
- `CMP r/m, imm + Jcc` (most common)
- `TEST r/m, imm + Jcc`
- `DEC/INC r/m + Jcc` (since Nehalem)
- `ADD/SUB r/m, imm + Jcc` (not fused -- confirmed by patent diagrams)

**Important:** The patent states fused pairs must be **adjacent in the decoder**. If the compiler inserts a `MOV` between `CMP` and `JCC`, fusion is broken. This supports our finding that compiler optimization is critical for achieving high fusion rates.

### ROB CAM Structure (US 9,116,688)

- ROB entries are content-addressable (CAM), not RAM
- Each entry has ~100 tags that are compared in parallel on wakeup
- Bitline capacitance scales with number of entries and number of ports
- The patent explicitly warns: "doubling ROB depth increases area by 3.7x - 5.2x depending on port count" -- confirming our O(n?) model

### TAGE Predictor (US 11,176,619)

- Geometric history: Table tags use history of length 4, 8, 16, 32, 64 (powers of 2)
- Tag computation is XOR-based (fast, 1 cycle)
- Useful counter: 3-bit saturating counter per entry
- The patent describes CSR (Corrected Skyhook Repair) for handling aliasing:
  - When two branches alias to the same entry, a 2-bit "useful" counter identifies the more common branch
  - The less common branch defaults to the bimodal predictor (T0)
- **Key for BEMI's pre-fill claim:** The patent says TAGE's accuracy degenerates when "stale useful counts are pre-loaded from external state" (column 28, line 43). Pre-filling may degrade accuracy if the static analysis is wrong.

---

## Limitations

1. **Patent analysis ? hardware verification.** Patents describe *intended* operation, not measured behavior. Actual hardware may differ.
2. **Focus on Intel.** AMD patents were not reviewed (time constraints). AMD's Zen architecture uses a different ?op cache format (larger: 64-80 bits compressed).
3. **Missing process node details.** Patents don't specify transistor sizes or power targets. Our area estimates are based on die shots and ISSCC papers, not patent data.

---

## Files

- Full patent PDFs: `patents/` (not included in repository -- patent office URLs only)
- Annotated diagrams: `patent_figures_annotated/` (in-progress, not published)

