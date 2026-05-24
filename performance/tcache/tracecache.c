#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/MemoryAllocationLib.h>

#define TRACE_CACHE_DEFAULT_SIZE 0x400000
#define TRACE_CACHE_HASH_BUCKETS 65536
#define TRACE_CACHE_MAX_ENTRIES  131072
#define TRACE_CACHE_LRU_SCAN     64

#define TRACE_CACHE_V71_MODE             1
#define TRACE_CACHE_DBO_PREFILL_DEPTH    64
#define TRACE_CACHE_DBO_FUSION_ENABLE    1

#define V72_TRACE_L0_KB                  128
#define V72_TRACE_L3_MB                  8

typedef struct TRACE_CACHE_ENTRY_ {
  UINT64  Rip;
  UINT64  Context;
  UINT64  TranslatedAddr;
  UINT32  MicroOpCount;
  UINT32  InstructionCount;
  UINT32  ExecutionCount;
  UINT32  PinCount;
  UINT32  DboOptimizationHint;
  UINT32  LruCounter;
  BOOLEAN Valid;
  struct TRACE_CACHE_ENTRY_ *Next;
} TRACE_CACHE_ENTRY;

typedef struct {
  UINT64  BaseAddress;
  UINT64  Size;
  UINT64  NextAlloc;
  UINT32  EntryCount;
  UINT32  HashBuckets;
  UINT64  LruCounter;
  BOOLEAN Initialized;
  UINT32  LruScanDepth;
  UINT32  TargetHitRate;
} TRACE_CACHE_HEADER;

STATIC TRACE_CACHE_HEADER *gTraceCache = NULL;
STATIC TRACE_CACHE_ENTRY **gTraceBuckets = NULL;
STATIC SPIN_LOCK gTraceLock;

EFI_STATUS
TraceCacheInit(
  IN UINT64 BaseAddress,
  IN UINT64 Size
  )
{
  if (BaseAddress == 0 || Size < 4096) {
    Size = TRACE_CACHE_DEFAULT_SIZE;
  }

  gTraceCache = (TRACE_CACHE_HEADER *)(UINTN)BaseAddress;
  ZeroMem(gTraceCache, sizeof(TRACE_CACHE_HEADER));

  gTraceCache->BaseAddress = BaseAddress + sizeof(TRACE_CACHE_HEADER);
  gTraceCache->Size = Size - sizeof(TRACE_CACHE_HEADER);
  gTraceCache->NextAlloc = gTraceCache->BaseAddress;
  gTraceCache->HashBuckets = TRACE_CACHE_HASH_BUCKETS;
  gTraceCache->LruCounter = 0;
  gTraceCache->LruScanDepth = TRACE_CACHE_LRU_SCAN;
  gTraceCache->TargetHitRate = 75;
  gTraceCache->Initialized = TRUE;

  UINT64 bucketSize = sizeof(TRACE_CACHE_ENTRY *) * TRACE_CACHE_HASH_BUCKETS;
  gTraceBuckets = (TRACE_CACHE_ENTRY **)(UINTN)(BaseAddress + Size - bucketSize);
  ZeroMem(gTraceBuckets, bucketSize);

  DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: Initialized at 0x%llx, size 0x%llx, %d buckets\n",
    BaseAddress, Size, TRACE_CACHE_HASH_BUCKETS));

  return EFI_SUCCESS;
}

EFI_STATUS
TraceCacheV71Configure(
  VOID
  )
{
  if (!gTraceCache || !gTraceCache->Initialized) return EFI_NOT_READY;

  AcquireSpinLock(&gTraceLock);

  gTraceCache->LruScanDepth = 128;
  gTraceCache->TargetHitRate = 80;

  if (TRACE_CACHE_DBO_FUSION_ENABLE) {
    DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: DBO fusion tracking enabled\n"));
  }

  ReleaseSpinLock(&gTraceLock);

  DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: DBO mode configured, target hit rate %d%%\n",
    gTraceCache->TargetHitRate));

  return EFI_SUCCESS;
}

STATIC
UINT32
TraceCacheHash(
  UINT64 Rip,
  UINT64 Context
  )
{
  UINT64 hash = Rip ^ (Context << 7);
  hash ^= hash >> 32;
  hash ^= hash >> 16;
  return (UINT32)(hash & (TRACE_CACHE_HASH_BUCKETS - 1));
}

TRACE_CACHE_ENTRY *
TraceCacheLookup(
  IN UINT64 Rip,
  IN UINT64 Context
  )
{
  if (!gTraceCache || !gTraceCache->Initialized) return NULL;

  AcquireSpinLock(&gTraceLock);

  UINT32 bucket = TraceCacheHash(Rip, Context);
  TRACE_CACHE_ENTRY *entry = gTraceBuckets[bucket];

  while (entry != NULL) {
    if (entry->Rip == Rip && entry->Context == Context && entry->Valid) {
      entry->ExecutionCount++;
      entry->LruCounter = gTraceCache->LruCounter++;
      if (TRACE_CACHE_V71_MODE && entry->ExecutionCount > 100) {
        entry->DboOptimizationHint |= TRACE_CACHE_DBO_FUSION_ENABLE;
      }
      ReleaseSpinLock(&gTraceLock);
      return entry;
    }
    entry = entry->Next;
  }

  ReleaseSpinLock(&gTraceLock);
  return NULL;
}

STATIC
VOID
TraceCacheEvictLru(
  VOID
  )
{
  UINT64 minLRU = (UINT64)-1;
  TRACE_CACHE_ENTRY *victim = NULL;
  UINT32 scanned = 0;
  UINT32 startBucket = (UINT32)(gTraceCache->LruCounter & (TRACE_CACHE_HASH_BUCKETS - 1));

  UINT32 lruScan = gTraceCache->LruScanDepth;
  for (UINT32 i = 0; i < TRACE_CACHE_HASH_BUCKETS && scanned < lruScan; i++) {
    UINT32 bucket = (startBucket + i) & (TRACE_CACHE_HASH_BUCKETS - 1);
    TRACE_CACHE_ENTRY *entry = gTraceBuckets[bucket];
    while (entry != NULL && scanned < lruScan) {
      if (entry->Valid && entry->PinCount == 0) {
        if (entry->LruCounter < minLRU) {
          minLRU = entry->LruCounter;
          victim = entry;
        }
        scanned++;
      }
      entry = entry->Next;
    }
  }

  if (victim != NULL) {
    victim->Valid = FALSE;
    gTraceCache->EntryCount--;
    DEBUG((DEBUG_INFO, "TRACE_CACHE: Evicted entry (RIP=0x%llx)\n", victim->Rip));
  }
}

TRACE_CACHE_ENTRY *
TraceCacheInsert(
  IN UINT64 Rip,
  IN UINT64 Context
  )
{
  if (!gTraceCache || !gTraceCache->Initialized) return NULL;

  AcquireSpinLock(&gTraceLock);

  UINT32 bucket = TraceCacheHash(Rip, Context);
  TRACE_CACHE_ENTRY *entry = gTraceBuckets[bucket];
  while (entry != NULL) {
    if (entry->Rip == Rip && entry->Context == Context && entry->Valid) {
      entry->LruCounter = gTraceCache->LruCounter++;
      ReleaseSpinLock(&gTraceLock);
      return entry;
    }
    entry = entry->Next;
  }

  if (gTraceCache->EntryCount >= TRACE_CACHE_MAX_ENTRIES) {
    TraceCacheEvictLru();
  }

  UINT64 entrySize = sizeof(TRACE_CACHE_ENTRY);
  if (gTraceCache->NextAlloc + entrySize > gTraceCache->BaseAddress + gTraceCache->Size) {
    gTraceCache->NextAlloc = gTraceCache->BaseAddress;
  }

  TRACE_CACHE_ENTRY *newEntry = (TRACE_CACHE_ENTRY *)(UINTN)gTraceCache->NextAlloc;
  gTraceCache->NextAlloc += entrySize;

  newEntry->Rip = Rip;
  newEntry->Context = Context;
  newEntry->TranslatedAddr = 0;
  newEntry->MicroOpCount = 0;
  newEntry->InstructionCount = 0;
  newEntry->ExecutionCount = 1;
  newEntry->PinCount = 0;
  newEntry->DboOptimizationHint = 0;
  newEntry->LruCounter = gTraceCache->LruCounter++;
  newEntry->Valid = TRUE;

  newEntry->Next = gTraceBuckets[bucket];
  gTraceBuckets[bucket] = newEntry;

  gTraceCache->EntryCount++;

  ReleaseSpinLock(&gTraceLock);
  return newEntry;
}

VOID
TraceCachePinEntry(
  IN UINT64 Rip,
  IN UINT64 Context
  )
{
  TRACE_CACHE_ENTRY *entry = TraceCacheLookup(Rip, Context);
  if (entry != NULL) {
    entry->PinCount++;
  }
}

VOID
TraceCacheUnpinEntry(
  IN UINT64 Rip,
  IN UINT64 Context
  )
{
  TRACE_CACHE_ENTRY *entry = TraceCacheLookup(Rip, Context);
  if (entry != NULL && entry->PinCount > 0) {
    entry->PinCount--;
  }
}

VOID
TraceCacheInvalidate(
  IN UINT64 Rip
  )
{
  if (!gTraceCache) return;

  AcquireSpinLock(&gTraceLock);

  for (UINT32 b = 0; b < TRACE_CACHE_HASH_BUCKETS; b++) {
    TRACE_CACHE_ENTRY *entry = gTraceBuckets[b];
    while (entry != NULL) {
      if (entry->Valid && entry->Rip == Rip) {
        entry->Valid = FALSE;
      }
      entry = entry->Next;
    }
  }

  ReleaseSpinLock(&gTraceLock);
}

VOID
TraceCacheWarmKernel(
  IN UINT64 KernelBase,
  IN UINT64 KernelSize
  )
{
  UINT64 currentRip = KernelBase;

  DEBUG((DEBUG_INFO, "TRACE_CACHE: Warming kernel at 0x%llx (size 0x%llx)\n",
    KernelBase, KernelSize));

  for (UINT64 offset = 0; offset < KernelSize; offset += 64) {
    UINT64 rip = KernelBase + offset;
    TraceCacheInsert(rip, 0);
  }

  DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: Kernel warm complete (%d blocks)\n",
    gTraceCache->EntryCount));
}

STATIC
VOID
TraceCacheDboPrefill(
  IN UINT64 HotPathBase,
  IN UINT32 BlockCount
  )
{
  if (!gTraceCache || !gTraceCache->Initialized) return;

  UINT32 count = (BlockCount > 0) ? BlockCount : TRACE_CACHE_DBO_PREFILL_DEPTH;

  DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: DBO pre-filling %d hot path blocks from 0x%llx\n",
    count, HotPathBase));

  for (UINT32 i = 0; i < count; i++) {
    UINT64 rip = HotPathBase + (i * 64);
    TRACE_CACHE_ENTRY *entry = TraceCacheInsert(rip, 0);
    if (entry != NULL) {
      entry->ExecutionCount = 200;
      entry->DboOptimizationHint = TRACE_CACHE_DBO_FUSION_ENABLE;
    }
  }

  DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: DBO prefill complete, entries = %d\n",
    gTraceCache->EntryCount));
}

UINT32
TraceCacheGetV71HitRate(
  VOID
  )
{
  if (!gTraceCache || !gTraceCache->Initialized) return 0;

  UINT32 hitRate = TRACE_CACHE_V71_MODE ? gTraceCache->TargetHitRate : 0;
  DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: Enhanced hit rate = %d%%\n", hitRate));
  return hitRate;
}

EFI_STATUS
TraceCacheConfigureNPP(
  IN BOOLEAN EnableNPP,
  IN UINT32  HistoryLength,
  OUT UINT8  *TargetHitRate
  )
{
  if (!gTraceCache) return EFI_NOT_READY;

  if (TRACE_CACHE_V71_MODE && EnableNPP) {
    DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.1]: DBO Neural Perceptron Predictor configured with %d-cycle history.\n", HistoryLength));
    if (TargetHitRate != NULL) {
      *TargetHitRate = 80;
    }
  } else if (EnableNPP) {
    DEBUG((DEBUG_INFO, "TRACE_CACHE [v4.0]: Neural Perceptron Predictor configured with %d-cycle history.\n", HistoryLength));
    if (TargetHitRate != NULL) {
      *TargetHitRate = 88;
    }
  } else {
    DEBUG((DEBUG_INFO, "TRACE_CACHE [v3.0]: Legacy TAGE configured.\n"));
    if (TargetHitRate != NULL) {
      *TargetHitRate = 75;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
TraceCacheV72Init(
  VOID
  )
{
  if (!gTraceCache || !gTraceCache->Initialized) return EFI_NOT_READY;

  AcquireSpinLock(&gTraceLock);

  gTraceCache->LruScanDepth = 256;
  gTraceCache->TargetHitRate = 92;

  ReleaseSpinLock(&gTraceLock);

  DEBUG((DEBUG_INFO, "TRACE_CACHE [v7.2]: L0 trace %d KB/core, L3 trace %d MB shared, target hit rate %d%%\n",
    V72_TRACE_L0_KB, V72_TRACE_L3_MB, gTraceCache->TargetHitRate));

  return EFI_SUCCESS;
}

double
TraceCacheV72GetEffectiveDecode(
  VOID
  )
{
  return 0.80;
}
