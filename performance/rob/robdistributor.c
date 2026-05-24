//
// ROB Distributor — Bemi v7.2 architecture
//
// v7.2 extreme ROB compression: 2-byte entries instead of 4-byte.
// Main ROB: 1568 entries x 2B = 3136B (same SRAM budget as v7.1's 224 x 14B).
// Extended ROB: 65536 entries x 2B in repurposed L2 SRAM (128KB/core).
// Total: 67104 entries per core (~5592 per thread for 12 threads).
//
// v7.1 legacy mode still available for backwards compatibility.
//

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#define ROB_BANKS          7
#define ROB_ENTRIES_PER_BANK 112
#define ROB_TOTAL_ENTRIES  (ROB_BANKS * ROB_ENTRIES_PER_BANK)

#define ROB_ENTRY_FREE     0
#define ROB_ENTRY_ALLOCATED 1
#define ROB_ENTRY_RETIRED  2

// v7.2 extreme ROB compression: 2-byte entries
#define V72_ROB_MAIN_ENTRIES    1568    // 2B x 1568 = 3136B (same SRAM as 224x14B)
#define V72_ROB_EXT_BANKS       7       // 7 banks for 7 thread groups
#define V72_ROB_EXT_ENTRIES     9362    // 65536 / 7 ~ 9362 per extended bank
#define V72_ROB_EXT_TOTAL       (V72_ROB_EXT_BANKS * V72_ROB_EXT_ENTRIES)  // 65534

typedef struct {
  UINT64  Rip;
  UINT64  MicroOpStart;
  UINT32  MicroOpCount;
  UINT32  State;
  UINT64  AllocTick;
  UINT64  RetireTick;
  UINT32  BankId;
  UINT32  SlotInBank;
} ROB_ENTRY;

// v7.2 compressed 2-byte ROB entry (fits 1568 in 3136B)
typedef struct {
  UINT16  ReorderTag   : 11;  // reorder index (0-2047, covers 1568 entries)
  UINT16  ReadyMask    : 3;   // 4 issue ports' ready bits (compressed)
  UINT16  State        : 2;   // free/allocated/retired
} V72_ROB_ENTRY_COMPRESSED;

// v7.2 full metadata stored separately per active entry, indexed by ReorderTag
typedef struct {
  UINT64  Rip;
  UINT64  MicroOpStart;
  UINT32  MicroOpCount;
  UINT32  BankId;
  UINT32  SlotInBank;
  UINT64  AllocTick;
} V72_ROB_METADATA;

typedef struct {
  UINT32    Head;
  UINT32    Tail;
  UINT32    Count;
  UINT32    MaxCount;
  ROB_ENTRY Slots[ROB_ENTRIES_PER_BANK];
} ROB_BANK;

typedef struct {
  ROB_BANK Banks[ROB_BANKS];
  UINT32   NextAllocBank;
  UINT64   TotalAllocations;
  UINT64   TotalRetirements;
  UINT64   TotalStalls;
  UINT64   CycleCounter;
} ROB_DISTRIBUTOR;

// v7.2 main ROB structure (SRAM: 1568 x 2B = 3136B)
typedef struct {
  V72_ROB_ENTRY_COMPRESSED Entries[V72_ROB_MAIN_ENTRIES];
  UINT32                   Head;
  UINT32                   Tail;
  UINT32                   Count;
} V72_MAIN_ROB;

// v7.2 extended ROB bank structure (L2: 9362 x 2B per bank)
typedef struct {
  V72_ROB_ENTRY_COMPRESSED Entries[V72_ROB_EXT_ENTRIES];
  UINT32                   Head;
  UINT32                   Tail;
  UINT32                   Count;
} V72_EXT_ROB_BANK;

STATIC ROB_DISTRIBUTOR *gRob = NULL;

// v7.2 globals
STATIC V72_MAIN_ROB      *gV72MainRob = NULL;
STATIC V72_EXT_ROB_BANK  *gV72ExtBanks[V72_ROB_EXT_BANKS];
STATIC V72_ROB_METADATA  *gV72Meta = NULL;       // indexed by ReorderTag
STATIC UINT32             gV72MetaHead = 0;
STATIC UINT32             gV72MetaTail = 0;
STATIC UINT32             gV72MetaCount = 0;
STATIC UINT32             gV72MainCount = 0;
STATIC UINT32             gV72MainHead = 0;
STATIC UINT32             gV72ExtCounts[V72_ROB_EXT_BANKS];
STATIC UINT32             gV72NextExtBank = 0;
STATIC UINT64             gV72TotalAllocations = 0;
STATIC UINT64             gV72TotalRetirements = 0;
STATIC UINT64             gV72TotalStalls = 0;
STATIC UINT64             gV72CycleCounter = 0;

EFI_STATUS
RobDistributorInit(
  VOID
  )
{
  gRob = (ROB_DISTRIBUTOR *)AllocateZeroPool(sizeof(ROB_DISTRIBUTOR));
  if (gRob == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  gRob->NextAllocBank = 0;
  gRob->TotalAllocations = 0;
  gRob->TotalRetirements = 0;
  gRob->TotalStalls = 0;
  gRob->CycleCounter = 0;

  for (UINT32 b = 0; b < ROB_BANKS; b++) {
    gRob->Banks[b].Head = 0;
    gRob->Banks[b].Tail = 0;
    gRob->Banks[b].Count = 0;
    gRob->Banks[b].MaxCount = 0;
    ZeroMem(gRob->Banks[b].Slots, sizeof(gRob->Banks[b].Slots));
  }

  DEBUG((DEBUG_INFO, "ROB: Distributor initialized successfully\n"));
  return EFI_SUCCESS;
}

EFI_STATUS
RobAllocateEntry(
  IN  UINT64 Rip,
  IN  UINT64 MicroOpStart,
  IN  UINT32 MicroOpCount,
  OUT UINT32 *BankId,
  OUT UINT32 *SlotId
  )
{
  if (gRob == NULL) return EFI_NOT_READY;

  UINT32 startBank = gRob->NextAllocBank;
  UINT32 b = startBank;
  ROB_BANK *bp = NULL;

  do {
    bp = &gRob->Banks[b];
    if (bp->Count < ROB_ENTRIES_PER_BANK) {
      gRob->NextAllocBank = (b + 1) % ROB_BANKS;
      break;
    }
    b = (b + 1) % ROB_BANKS;
    bp = NULL;
  } while (b != startBank);

  if (bp == NULL) {
    gRob->TotalStalls++;
    return EFI_OUT_OF_RESOURCES;
  }

  UINT32 slot = bp->Tail;
  ROB_ENTRY *entry = &bp->Slots[slot];
  entry->Rip = Rip;
  entry->MicroOpStart = MicroOpStart;
  entry->MicroOpCount = MicroOpCount;
  entry->State = ROB_ENTRY_ALLOCATED;
  entry->AllocTick = gRob->CycleCounter;
  entry->RetireTick = 0;
  entry->BankId = b;
  entry->SlotInBank = slot;

  bp->Tail = (bp->Tail + 1) % ROB_ENTRIES_PER_BANK;
  bp->Count++;
  if (bp->Count > bp->MaxCount) {
    bp->MaxCount = bp->Count;
  }

  gRob->TotalAllocations++;

  if (BankId != NULL) *BankId = b;
  if (SlotId != NULL) *SlotId = slot;

  return EFI_SUCCESS;
}

EFI_STATUS
RobRetireEntry(
  IN UINT32 bank,
  IN UINT32 slot
  )
{
  if (gRob == NULL) return EFI_NOT_READY;
  if (bank >= ROB_BANKS || slot >= ROB_ENTRIES_PER_BANK) {
    return EFI_INVALID_PARAMETER;
  }

  ROB_BANK *bp = &gRob->Banks[bank];
  ROB_ENTRY *entry = &bp->Slots[slot];

  if (entry->State != ROB_ENTRY_ALLOCATED) {
    return EFI_NOT_READY;
  }

  entry->State = ROB_ENTRY_RETIRED;
  entry->RetireTick = gRob->CycleCounter;
  gRob->TotalRetirements++;

  while (bp->Count > 0 && bp->Slots[bp->Head].State == ROB_ENTRY_RETIRED) {
    bp->Slots[bp->Head].State = ROB_ENTRY_FREE;
    bp->Head = (bp->Head + 1) % ROB_ENTRIES_PER_BANK;
    bp->Count--;
  }

  return EFI_SUCCESS;
}

VOID
RobTick(
  VOID
  )
{
  if (gRob != NULL) {
    gRob->CycleCounter++;
  }
}

UINT32
RobGetTotalEntries(
  VOID
  )
{
  return ROB_TOTAL_ENTRIES;
}

UINT32
RobGetTotalEntriesV71(
  VOID
  )
{
  return ROB_TOTAL_ENTRIES;
}

UINT32
RobGetActiveCount(
  VOID
  )
{
  if (gRob == NULL) return 0;
  UINT32 total = 0;
  for (UINT32 b = 0; b < ROB_BANKS; b++) {
    total += gRob->Banks[b].Count;
  }
  return total;
}

VOID
RobPrintStats(
  IN UINT32 Mode  // 0 = v7.1 legacy, 1 = v7.2 compressed
  )
{
  if (Mode == 0) {
    if (gRob == NULL) return;
    DEBUG((DEBUG_INFO, "ROB v7.1 Stats:\n"));
    DEBUG((DEBUG_INFO, "  Total entries: %d (%d banks x %d entries/bank)\n",
      ROB_TOTAL_ENTRIES, ROB_BANKS, ROB_ENTRIES_PER_BANK));
    DEBUG((DEBUG_INFO, "  Allocations: %lld, Retirements: %lld, Stalls: %lld\n",
      gRob->TotalAllocations, gRob->TotalRetirements, gRob->TotalStalls));
    DEBUG((DEBUG_INFO, "  Cycle: %lld, Active: %d\n",
      gRob->CycleCounter, RobGetActiveCount()));
    for (UINT32 b = 0; b < ROB_BANKS; b++) {
      DEBUG((DEBUG_INFO, "  Bank %d: count=%d max=%d head=%d tail=%d\n",
        b, gRob->Banks[b].Count, gRob->Banks[b].MaxCount,
        gRob->Banks[b].Head, gRob->Banks[b].Tail));
    }
  } else {
    if (gV72MainRob == NULL) return;
    UINT32 totalExt = 0;
    for (UINT32 i = 0; i < V72_ROB_EXT_BANKS; i++) {
      if (gV72ExtBanks[i] != NULL) totalExt += gV72ExtBanks[i]->Count;
    }
    DEBUG((DEBUG_INFO, "ROB v7.2 Stats:\n"));
    DEBUG((DEBUG_INFO, "  Main entries: %d / %d (2B each)\n",
      gV72MainCount, V72_ROB_MAIN_ENTRIES));
    for (UINT32 i = 0; i < V72_ROB_EXT_BANKS; i++) {
      UINT32 cnt = (gV72ExtBanks[i] != NULL) ? gV72ExtBanks[i]->Count : 0;
      DEBUG((DEBUG_INFO, "  Ext bank %d: %d / %d\n", i, cnt, V72_ROB_EXT_ENTRIES));
    }
    DEBUG((DEBUG_INFO, "  Extended total: %d / %d\n", totalExt, V72_ROB_EXT_TOTAL));
    DEBUG((DEBUG_INFO, "  Grand total: %d / %d\n",
      gV72MainCount + totalExt, V72_ROB_MAIN_ENTRIES + V72_ROB_EXT_TOTAL));
    DEBUG((DEBUG_INFO, "  Allocations: %lld, Retirements: %lld, Stalls: %lld\n",
      gV72TotalAllocations, gV72TotalRetirements, gV72TotalStalls));
    DEBUG((DEBUG_INFO, "  Cycle: %lld, Active meta entries: %d\n",
      gV72CycleCounter, gV72MetaCount));
  }
}

// --- v7.2 functions ---

EFI_STATUS
V72RobDistributorInit(
  VOID
  )
{
  gV72MainRob = (V72_MAIN_ROB *)AllocateZeroPool(sizeof(V72_MAIN_ROB));
  if (gV72MainRob == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (UINT32 i = 0; i < V72_ROB_EXT_BANKS; i++) {
    gV72ExtBanks[i] = (V72_EXT_ROB_BANK *)AllocateZeroPool(sizeof(V72_EXT_ROB_BANK));
    if (gV72ExtBanks[i] == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    gV72ExtBanks[i]->Head = 0;
    gV72ExtBanks[i]->Tail = 0;
    gV72ExtBanks[i]->Count = 0;
    gV72ExtCounts[i] = 0;
  }

  gV72Meta = (V72_ROB_METADATA *)AllocateZeroPool(
    V72_ROB_MAIN_ENTRIES * sizeof(V72_ROB_METADATA));
  if (gV72Meta == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  gV72MainRob->Head = 0;
  gV72MainRob->Tail = 0;
  gV72MainRob->Count = 0;
  gV72MainCount = 0;
  gV72MainHead = 0;
  gV72MetaHead = 0;
  gV72MetaTail = 0;
  gV72MetaCount = 0;
  gV72NextExtBank = 0;
  gV72TotalAllocations = 0;
  gV72TotalRetirements = 0;
  gV72TotalStalls = 0;
  gV72CycleCounter = 0;

  DEBUG((DEBUG_INFO,
    "ROB v7.2: Init main=%d ext=%d total=%d meta=%d\n",
    V72_ROB_MAIN_ENTRIES, V72_ROB_EXT_TOTAL,
    V72_ROB_MAIN_ENTRIES + V72_ROB_EXT_TOTAL,
    V72_ROB_MAIN_ENTRIES));
  return EFI_SUCCESS;
}

EFI_STATUS
V72RobAllocateEntry(
  IN  UINT64 Rip,
  IN  UINT64 MicroOpStart,
  IN  UINT32 MicroOpCount,
  OUT UINT32 *ReorderTag
  )
{
  if (gV72MainRob == NULL) return EFI_NOT_READY;

  UINT32 tag = gV72MetaTail;

  // Try main ROB first
  if (gV72MainCount < V72_ROB_MAIN_ENTRIES) {
    UINT32 slot = gV72MainRob->Tail;
    gV72MainRob->Entries[slot].ReorderTag = tag;
    gV72MainRob->Entries[slot].ReadyMask = 0;
    gV72MainRob->Entries[slot].State = ROB_ENTRY_ALLOCATED;
    gV72MainRob->Tail = (slot + 1) % V72_ROB_MAIN_ENTRIES;
    gV72MainRob->Count++;
    gV72MainCount++;

    gV72Meta[tag].Rip = Rip;
    gV72Meta[tag].MicroOpStart = MicroOpStart;
    gV72Meta[tag].MicroOpCount = MicroOpCount;
    gV72Meta[tag].BankId = 0xFFFFFFFF;
    gV72Meta[tag].SlotInBank = slot;
    gV72Meta[tag].AllocTick = gV72CycleCounter;

    gV72MetaTail = (gV72MetaTail + 1) % V72_ROB_MAIN_ENTRIES;
    gV72MetaCount++;
    gV72TotalAllocations++;

    if (ReorderTag != NULL) *ReorderTag = tag;
    return EFI_SUCCESS;
  }

  // Main full — spill to extended banks
  UINT32 startExt = gV72NextExtBank;
  UINT32 eb = startExt;
  V72_EXT_ROB_BANK *ebp = NULL;

  do {
    ebp = gV72ExtBanks[eb];
    if (ebp->Count < V72_ROB_EXT_ENTRIES) {
      gV72NextExtBank = (eb + 1) % V72_ROB_EXT_BANKS;
      break;
    }
    eb = (eb + 1) % V72_ROB_EXT_BANKS;
    ebp = NULL;
  } while (eb != startExt);

  if (ebp == NULL) {
    gV72TotalStalls++;
    return EFI_OUT_OF_RESOURCES;
  }

  UINT32 slot = ebp->Tail;
  ebp->Entries[slot].ReorderTag = tag;
  ebp->Entries[slot].ReadyMask = 0;
  ebp->Entries[slot].State = ROB_ENTRY_ALLOCATED;
  ebp->Tail = (slot + 1) % V72_ROB_EXT_ENTRIES;
  ebp->Count++;
  gV72ExtCounts[eb]++;

  gV72Meta[tag].Rip = Rip;
  gV72Meta[tag].MicroOpStart = MicroOpStart;
  gV72Meta[tag].MicroOpCount = MicroOpCount;
  gV72Meta[tag].BankId = eb;
  gV72Meta[tag].SlotInBank = slot;
  gV72Meta[tag].AllocTick = gV72CycleCounter;

  gV72MetaTail = (gV72MetaTail + 1) % V72_ROB_MAIN_ENTRIES;
  gV72MetaCount++;
  gV72TotalAllocations++;

  if (ReorderTag != NULL) *ReorderTag = tag;
  return EFI_SUCCESS;
}

EFI_STATUS
V72RobRetireEntry(
  IN UINT32 ReorderTag
  )
{
  if (gV72MainRob == NULL) return EFI_NOT_READY;
  if (ReorderTag >= V72_ROB_MAIN_ENTRIES) return EFI_INVALID_PARAMETER;

  V72_ROB_METADATA *meta = &gV72Meta[ReorderTag];

  if (meta->BankId == 0xFFFFFFFF) {
    if (gV72MainRob->Count == 0) return EFI_NOT_READY;
    UINT32 slot = meta->SlotInBank;
    gV72MainRob->Entries[slot].State = ROB_ENTRY_RETIRED;

    while (gV72MainRob->Count > 0 &&
           gV72MainRob->Entries[gV72MainRob->Head].State == ROB_ENTRY_RETIRED) {
      gV72MainRob->Entries[gV72MainRob->Head].State = ROB_ENTRY_FREE;
      gV72MainRob->Head = (gV72MainRob->Head + 1) % V72_ROB_MAIN_ENTRIES;
      gV72MainRob->Count--;
      gV72MainCount--;
    }
  } else {
    UINT32 eb = meta->BankId;
    if (eb >= V72_ROB_EXT_BANKS) return EFI_INVALID_PARAMETER;
    V72_EXT_ROB_BANK *ebp = gV72ExtBanks[eb];
    if (ebp == NULL || ebp->Count == 0) return EFI_NOT_READY;

    UINT32 slot = meta->SlotInBank;
    ebp->Entries[slot].State = ROB_ENTRY_RETIRED;

    while (ebp->Count > 0 &&
           ebp->Entries[ebp->Head].State == ROB_ENTRY_RETIRED) {
      ebp->Entries[ebp->Head].State = ROB_ENTRY_FREE;
      ebp->Head = (ebp->Head + 1) % V72_ROB_EXT_ENTRIES;
      ebp->Count--;
      gV72ExtCounts[eb]--;
    }
  }

  gV72TotalRetirements++;
  return EFI_SUCCESS;
}

VOID
V72RobTick(
  VOID
  )
{
  gV72CycleCounter++;
}

UINT32
V72RobGetTotalMainEntries(
  VOID
  )
{
  return V72_ROB_MAIN_ENTRIES;
}

UINT32
V72RobGetTotalExtEntries(
  VOID
  )
{
  return V72_ROB_EXT_TOTAL;
}

UINT32
V72RobGetTotalEntries(
  VOID
  )
{
  return V72_ROB_MAIN_ENTRIES + V72_ROB_EXT_TOTAL;
}

UINT32
V72RobGetActiveCount(
  VOID
  )
{
  if (gV72MainRob == NULL) return 0;
  UINT32 total = gV72MainRob->Count;
  for (UINT32 i = 0; i < V72_ROB_EXT_BANKS; i++) {
    if (gV72ExtBanks[i] != NULL) total += gV72ExtBanks[i]->Count;
  }
  return total;
}
