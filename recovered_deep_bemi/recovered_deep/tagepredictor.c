#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define TAGE_TAG_BITS    12
#define TAGE_USEFUL_BITS 3
#define TAGE_TABLE_COUNT 4
#define TAGE_ENTRIES_PER_TABLE 4096
#define BTB_ENTRIES 8192

typedef struct {
  UINT64  Tag;
  UINT8   Useful : 3;
  UINT8   Taken  : 1;
  UINT8   Valid  : 1;
  UINT8   Pad    : 3;
} TAGE_ENTRY;

typedef struct {
  UINT64  TargetRip;
  UINT64  TargetAddress;
  UINT64  Tag;
  UINT8   Valid : 1;
  UINT8   Pad   : 7;
} BTB_ENTRY;

STATIC TAGE_ENTRY gTageTables[TAGE_TABLE_COUNT][TAGE_ENTRIES_PER_TABLE];
STATIC BTB_ENTRY  gBtb[BTB_ENTRIES];
STATIC UINT64     gTagePredictions = 0;
STATIC UINT64     gTageCorrect = 0;
STATIC UINT64     gTageMispredictions = 0;

VOID
TagePredictorInit(
  VOID
  )
{
  ZeroMem(gTageTables, sizeof(gTageTables));
  ZeroMem(gBtb, sizeof(gBtb));

  gTagePredictions = 0;
  gTageCorrect = 0;
  gTageMispredictions = 0;

  DEBUG((DEBUG_INFO, "TAGE: Predictor initialized (%d tables x %d entries, %d BTB entries)\n",
    TAGE_TABLE_COUNT, TAGE_ENTRIES_PER_TABLE, BTB_ENTRIES));
}

STATIC
UINT32
TageIndex(
  UINT64 Rip,
  UINT32 Table
  )
{
  UINT32 shift = Table * 4 + 2;
  return (UINT32)((Rip >> shift) & (TAGE_ENTRIES_PER_TABLE - 1));
}

STATIC
UINT64
TageTag(
  UINT
<truncated 128 bytes>
;
        entry->Taken = Taken;
        entry->Useful = 0;
      }
    } else {
      if (entry->Taken == Taken) {
        if (entry->Useful < 7) entry->Useful++;
        gTageCorrect++;
      } else {
        if (entry->Useful > 0) entry->Useful--;
        entry->Taken = Taken;
        gTageMispredictions++;
      }
    }
  }

  UINT32 btbIdx = (UINT32)(Rip & (BTB_ENTRIES - 1));
  gBtb[btbIdx].TargetRip = Rip;
  gBtb[btbIdx].TargetAddress = Target;
  gBtb[btbIdx].Valid = 1;
  gBtb[btbIdx].Tag = Rip >> 12;
}

VOID
TagePrefillKernel(
  IN UINT64 KernelBase,
  IN UINT64 KernelSize,
  IN UINT64 *BranchTargets,
  IN UINT32 BranchCount
  )
{
  for (UINT32 i = 0; i < BranchCount; i++) {
    UINT64 branchRip = KernelBase + i * 16;
    UINT64 target = BranchTargets[i];

    for (INT32 t = 0; t < TAGE_TABLE_COUNT; t++) {
      UINT32 idx = TageIndex(branchRip, t);
      UINT64 tag = TageTag(branchRip, t);
      TAGE_ENTRY *entry = &gTageTables[t][idx];

      entry->Tag = tag;
      entry->Valid = 1;
      entry->Taken = (target > branchRip);
      entry->Useful = 2;
    }

    UINT32 btbIdx = (UINT32)(branchRip & (BTB_ENTRIES - 1));
    gBtb[btbIdx].TargetRip = branchRip;
    gBtb[btbIdx].TargetAddress = target;
    gBtb[btbIdx].Valid = 1;
    gBtb[btbIdx].Tag = branchRip >> 12;
  }

  DEBUG((DEBUG_INFO, "TAGE: Pre-filled %d kernel branch entries\n", BranchCount));
}

VOID
TageGetStats(
  OUT UINT64 *Predictions,
  OUT UINT64 *Correct,
  OUT UINT64 *Mispredictions
  )
{
  *Predictions = gTagePredictions;
  *Correct = gTageCorrect;
  *Mispredictions = gTageMispredictions;
}
