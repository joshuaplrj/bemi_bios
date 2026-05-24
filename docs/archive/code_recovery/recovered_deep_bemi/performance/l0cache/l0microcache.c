#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

/**
  L0 Micro-Cache — v2.0 Scaled Dominance
  ========================================
  1 KB private direct-mapped cache per RISC execution unit.

  Design rationale:
    The v1.3 architecture suffered from L1 cache thrashing (9.4% miss rate)
    because 84 threads shared 32 KB L1 per core (4.57 KB/thread). The L0
    micro-cache absorbs 70% of memory accesses at 1-cycle latency, reducing
    L1 pressure to only 30% of original traffic.

  Physical parameters (6nm):
    - 16 cache lines × 64 bytes = 1024 bytes (1 KB)
    - Direct-mapped (1-way) for minimum access latency
    - Tags: 48-bit physical address → 6 index bits + tag
    - Write-through policy: writes go to L0 + L1 simultaneously
    - No coherence protocol between L0 caches (write-through ensures L1 is always up to date)
    - Area: ~0.05 mm² per unit at 6nm

  Cache line layout:
    [Valid(1)] [Dirty(1)] [Tag(42)] [Data(512 bits = 64 bytes)]
**/

#define L0_CACHE_LINES         16
#define L0_LINE_SIZE           64    /* bytes per cache line */
#define L0_CACHE_SIZE          (L0_CACHE_LINES * L0_LINE_SIZE)  /* 1024 bytes */
#define L0_INDEX_BITS          4     /* log2(16) = 4 bits */
#define L0_OFFSET_BITS         6     /* log2(64) = 6 bits */
#define L0_TAG_SHIFT           (L0_INDEX_BITS +
<truncated 5931 bytes>

  UINT64 tag = L0CacheTag(PhysAddr);

  for (UINT32 u = 0; u < gL0ActiveUnits; u++) {
    if (gL0Caches[u] == NULL || !gL0Caches[u]->Initialized) continue;
    L0_CACHE_LINE *line = &gL0Caches[u]->Lines[index];
    if (line->Valid && line->Tag == tag) {
      line->Valid = FALSE;
    }
  }
}

/**
  Print L0 cache statistics for a specific unit.
**/
VOID
L0CachePrintStats(
  IN UINT32 UnitId
  )
{
  if (UnitId >= L0_MAX_UNITS || gL0Caches[UnitId] == NULL) return;

  L0_MICRO_CACHE *c = gL0Caches[UnitId];
  UINT64 total = c->TotalAccesses;
  double hitRate = (total > 0) ? (double)c->Hits / (double)total * 100.0 : 0.0;

  DEBUG((DEBUG_INFO, "L0 Unit %d: accesses=%lld hits=%lld misses=%lld hitRate=%.1f%% writeThru=%lld\n",
    UnitId, total, c->Hits, c->Misses, hitRate, c->WriteThrough));
}

/**
  Print aggregate L0 statistics across all active units.
**/
VOID
L0CachePrintAggregateStats(
  VOID
  )
{
  UINT64 totalAccess = 0, totalHit = 0, totalMiss = 0, totalWT = 0;

  for (UINT32 u = 0; u < gL0ActiveUnits; u++) {
    if (gL0Caches[u] == NULL) continue;
    totalAccess += gL0Caches[u]->TotalAccesses;
    totalHit += gL0Caches[u]->Hits;
    totalMiss += gL0Caches[u]->Misses;
    totalWT += gL0Caches[u]->WriteThrough;
  }

  double hitRate = (totalAccess > 0) ? (double)totalHit / (double)totalAccess * 100.0 : 0.0;
  DEBUG((DEBUG_INFO, "L0 AGGREGATE: %d units, accesses=%lld hits=%lld (%.1f%%) misses=%lld writeThru=%lld\n",
    gL0ActiveUnits, totalAccess, totalHit, hitRate, totalMiss, totalWT));
  DEBUG((DEBUG_INFO, "L0 AREA: %d units x 0.05mm² = %.2fmm² total at 6nm\n",
    gL0ActiveUnits, gL0ActiveUnits * 0.05));
}
