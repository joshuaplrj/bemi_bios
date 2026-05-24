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
  UINT64 Rip,
  UINT32 Table
  )
{
  UINT32 shift = Table * 4 + 6;
  return (Rip >> shift) ^ (Rip & ((1 << TAGE_TAG_BITS) - 1));
}

BOOLEAN
TagePredict(
  IN  UINT64  Rip,
  OUT UINT64  *Target
  )
{
  // 1. Look up in TAGE tables (table 3 down to 0)
  for (INT32 t = TAGE_TABLE_COUNT - 1; t >= 0; t--) {
    UINT32 idx = TageIndex(Rip, t);
    UINT64 tag = TageTag(Rip, t);
    TAGE_ENTRY *entry = &gTageTables[t][idx];
    if (entry->Valid && entry->Tag == tag) {
      if (Target != NULL) {
        UINT32 btbIdx = (UINT32)(Rip & (BTB_ENTRIES - 1));
        if (gBtb[btbIdx].Valid && gBtb[btbIdx].TargetRip == Rip) {
          *Target = gBtb[btbIdx].TargetAddress;
        } else {
          *Target = Rip + 4;
        }
      }
      return entry->Taken;
    }
  }

  // 2. Fall back to BTB prediction
  UINT32 btbIdx = (UINT32)(Rip & (BTB_ENTRIES - 1));
  if (gBtb[btbIdx].Valid && gBtb[btbIdx].TargetRip == Rip) {
    if (Target != NULL) {
      *Target = gBtb[btbIdx].TargetAddress;
    }
    return TRUE;
  }

  if (Target != NULL) {
    *Target = Rip + 4;
  }
  return FALSE;
}

VOID
TageUpdate(
  IN UINT64  Rip,
  IN BOOLEAN Taken,
  IN UINT64  Target
  )
{
  gTagePredictions++;

  INT32 predictingTable = -1;
  for (INT32 t = TAGE_TABLE_COUNT - 1; t >= 0; t--) {
    UINT32 idx = TageIndex(Rip, t);
    UINT64 tag = TageTag(Rip, t);
    TAGE_ENTRY *entry = &gTageTables[t][idx];
    if (entry->Valid && entry->Tag == tag) {
      predictingTable = t;
      break;
    }
  }

  if (predictingTable != -1) {
    UINT32 idx = TageIndex(Rip, predictingTable);
    TAGE_ENTRY *entry = &gTageTables[predictingTable][idx];
    if (entry->Taken == Taken) {
      if (entry->Useful < 7) entry->Useful++;
      gTageCorrect++;
    } else {
      if (entry->Useful > 0) {
        entry->Useful--;
      } else {
        entry->Taken = Taken;
      }
      gTageMispredictions++;
    }
  } else {
    // Allocate new entry
    for (INT32 t = 0; t < TAGE_TABLE_COUNT; t++) {
      UINT32 idx = TageIndex(Rip, t);
      UINT64 tag = TageTag(Rip, t);
      TAGE_ENTRY *entry = &gTageTables[t][idx];
      if (!entry->Valid || entry->Useful == 0) {
        entry->Tag = tag;
        entry->Valid = 1;
        entry->Taken = Taken;
        entry->Useful = 0;
        break;
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
