#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#define ROB_BANKS          6
#define ROB_ENTRIES_PER_BANK 14
#define ROB_TOTAL_ENTRIES  (ROB_BANKS * ROB_ENTRIES_PER_BANK)

#define ROB_ENTRY_FREE     0
#define ROB_ENTRY_ALLOCATED 1
#define ROB_ENTRY_RETIRED  2

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

STATIC ROB_DISTRIBUTOR *gRob = NULL;

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
    gRob->Banks
<truncated 2036 bytes>

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
  VOID
  )
{
  if (gRob == NULL) return;

  DEBUG((DEBUG_INFO, "ROB v1.3 Stats:\n"));
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
}
