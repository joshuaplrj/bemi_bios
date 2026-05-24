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

The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.