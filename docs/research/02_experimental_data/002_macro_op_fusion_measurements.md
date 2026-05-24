# Experimental Data -- Macro-Op Fusion Bonus Measurements

**Date:** 2024-10-25
**Status:** Preliminary -- measured on Intel hardware only

---

## Background

The published docs claim a 1.3x fusion bonus for BEMI's macro-op fusion pipeline. This is based on Intel's documented macro-op fusion behavior, not on BEMI-specific hardware (which doesn't exist yet). This document captures the measurement approach and underlying assumptions.

## Intel Macro-Op Fusion Reference

Intel's Sandy Bridge and later microarchitectures fuse specific instruction pairs:

| Fused Pair | Intel Support | Notes |
|---|---|---|
| `CMP r/m, imm` + `Jcc` | Yes | Most common; accounts for ~60% of fusions |
| `TEST r/m, imm` + `Jcc` | Yes | Similar to CMP |
| `ADD r/m, imm` + `Jcc` | No (but `DEC/Jcc` on some) | Not fused unless DEC |
| `DEC/INC r/m` + `Jcc` | Yes (since Nehalem) | Legacy support |
| `SUB r/m, imm` + `Jcc` | No | Would be useful but not implemented |

**Claimed fusion benefit:** Each fused pair saves one ?op in the pipeline. Intel documentation shows fusion reduces ?op count by ~5-15% on typical code. Our claim of 30% (1.3x) is 2-6x higher than Intel's observed benefit.

## Why 1.3x Is Plausible (and Why It's Not)

### Case For 1.3x

1. **BEMI's compiler controls the instruction stream.** Unlike x86 binaries compiled by GCC/LLVM without fusion awareness, BEMI's compiler can deliberately reorder code to maximize fusion opportunities. It can arrange comparisons and branches to be adjacent.
2. **BEMI's "Weaponized" mode keeps the x86 decoder.** The decoder already has fusion logic. BEMI just feeds it more fusible pairs.
3. **The theoretical maximum fusion rate** for a well-structured loop is ~50% of all CMP/JCC pairs being adjacent. If 60% of branches are conditional jumps following a compare, and 50% of those can be fused, then `0.6 * 0.5 = 0.3` (30%) of branch-related ?ops are saved. If branches account for ~20% of all ?ops, total ?op reduction is `0.3 * 0.2 = 0.06` -- only 6%.

This suggests the 1.3x claim may be too aggressive.

### Revised Estimate

Let's reconstruct the 1.3x fusion bonus more carefully:

- Baseline: 1000 x86 instructions -> ~1000 ?ops (after x86 decoder)
- Fused pairs: `CMP + JCC` fused into 1 macro-op instead of 2 ?ops -> saves 1 ?op per fusion
- Fusion rate depends on adjacency in the BEMI-compiled instruction stream

| Scenario | Fusion % of branches | Total ?ops | IPC (4-wide) | Fusion Bonus |
|---|---|---|---|---|
| No fusion (x86 baseline) | 0% | 1000 | 1.0 | 1.0x |
| Intel typical | 15% | 970 | 1.03 | 1.03x |
| BEMI-optimistic | 30% | 940 | 1.06 | 1.06x |
| BEMI-aggressive | 60% | 880 | 1.14 | 1.14x |
| Theoretical max | 100% | 760 | 1.32 | 1.32x |

**The 1.3x fusion bonus is only achievable near the theoretical maximum fusion rate.** This requires:
1. Every CMP/JCC pair is adjacent (compiler-controlled)
2. Every memory-operand CMP/JCC is converted to register-operand (avoids LOAD ?ops)
3. All branches are conditional (no indirect calls in the hot path)

### Practical Expectation

Our benchmark suite currently uses a fixed `BEMI_FUSION = 1.3` constant (in `bemi_constants.py`). For the production model, this should be **revised to 1.1x - 1.15x** based on realistic fusion rates.

**However:** The published 7.8x throughput advantage is only partially sensitive to the fusion bonus. Removing fusion entirely (1.0x) reduces total throughput from `1.3 * 144 = 187.2` to `1.0 * 144 = 144.0` -- still a 6.0x improvement over x86's 24.0. The fusion bonus sweetens the deal but isn't the foundation.

## Raw Measurement: Fusion Detection via perf

We attempted to measure fusion rates empirically by running a CMP/JCC-heavy loop and counting ?op vs instruction ratios via `perf stat`:

```
perf stat -e uops_issued.any,instructions ./fusion_test_loop
```

Results (Intel i7-13700H):

| Metric | Count |
|---|---|
| Instructions | 1,200,000,000 |
| uops_issued.any | 1,140,000,000 |
| ?op/instruction ratio | 0.95 |
| Estimated fusion rate | ~5% |

**Analysis:** The 0.95 ?op/instruction ratio indicates minimal fusion. Most fused pairs in standard GCC output are CMP/JCC, which account for a small fraction of total ?ops.

## Conclusion

- The 1.3x fusion bonus in the published model is **optimistic** by about 15-20 percentage points
- A realistic range is **1.05x - 1.15x** depending on compiler optimization
- The BEMI architecture does not depend on the fusion bonus for its core advantage (thread density)
- Recommend updating `BEMI_FUSION` to 1.15x for conservative estimates, and documenting the uncertainty

## Open Question

Can the BEMI compiler be modified to intentionally insert CMP/JCC adjacency? If yes, 1.2x may be achievable. This requires compiler-level research (outside current scope).

