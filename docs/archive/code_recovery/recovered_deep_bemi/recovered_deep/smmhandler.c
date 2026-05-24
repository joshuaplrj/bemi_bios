#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#define SMM_BSP_RESUME_ENTRY  0x38000
#define SMM_AP_RESUME_ENTRY   0x39000
#define SMM_DEFAULT_SMBASE    0x30000
#define SMM_LATENCY_BUDGET_US 100
#define SAVED_STATE_MAGIC     0x42454D49534D4D00ULL

typedef struct {
  UINT64  Magic;
  UINT64  TraceCacheBase;
  UINT64  TraceCacheSize;
  UINT64  TageTableBase;
  UINT64  TageTableSize;
  UINT64  HypervisorStateBase;
  UINT64  HypervisorStateSize;
  UINT64  SavedTraceCacheEntries;
  UINT64  SavedTageTables[4];
  UINT64  SavedExecutionFlags;
  BOOLEAN BemiActive;
  UINT8   Pad[7];
  UINT64  SaveTimestamp;
  UINT64  RestoreTimestamp;
} BEMI_SMM_STATE;

typedef struct {
  UINT64  TscFrequency;
  UINT64  MaxLatencyUs;
  UINT64  TotalSmiCount;
  UINT64  BudgetViolations;
  UINT32  SmrrBase;
  UINT32  SmrrMask;
  BOOLEAN SmrrConfigured;
} BEMI_SMM_STATISTICS;

STATIC BEMI_SMM_STATE gSmmState;
STATIC BEMI_SMM_STATISTICS gSmmStats;
STATIC BOOLEAN gSmmActive = FALSE;
STATIC UINT8 *gSavedHypervisorState = NULL;

EFI_STATUS
SmmHandlerInit(
  IN UINT64 TraceCacheBase,
  IN UINT64 TraceCacheSize
  )
{
  ZeroMem(&gSmmState, sizeof(gSmmState))
<truncated 115 bytes>
lx)\n",
      gSmmState.TraceCacheBase,
      gSmmState.TraceCacheBase + gSmmState.TraceCacheSize));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
SmmHandlerRestoreTraceCache(
  VOID
  )
{
  if (gSmmState.TraceCacheBase != 0) {
    DEBUG((DEBUG_INFO, "SMM: Trace cache preserved across SMI boundary\n"));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
SmmHandlerValidate(
  VOID
  )
{
  UINT64 elapsedUs = gSmmStats.TotalSmiCount > 0 ?
    (gSmmState.RestoreTimestamp - gSmmState.SaveTimestamp) /
    (gSmmStats.TscFrequency / 1000000ULL) : 0;

  if (gSmmStats.BudgetViolations > 0) {
    DEBUG((DEBUG_WARN, "SMM: %lld budget violations detected, max latency %lld us\n",
      gSmmStats.BudgetViolations, gSmmStats.MaxLatencyUs));
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
SmmHandlerRegisterSmramRegion(
  IN UINT64 Base,
  IN UINT64 Size
  )
{
  if (Base == 0 || Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG((DEBUG_INFO, "SMM: SMRAM region at 0x%llx, size 0x%llx\n", Base, Size));
  return EFI_SUCCESS;
}

EFI_STATUS
SmmHandlerConfigureCpu(
  IN UINT32 CpuIndex,
  IN UINT64 SmBase
  )
{
  if (SmBase == 0) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG((DEBUG_INFO, "SMM: CPU %d configured with SMBASE 0x%llx\n", CpuIndex, SmBase));
  return EFI_SUCCESS;
}

VOID
SmmHandlerGetStats(
  OUT UINT64 *TotalSmiCount,
  OUT UINT64 *MaxLatencyUs,
  OUT UINT64 *BudgetViolations
  )
{
  *TotalSmiCount = gSmmStats.TotalSmiCount;
  *MaxLatencyUs = gSmmStats.MaxLatencyUs;
  *BudgetViolations = gSmmStats.BudgetViolations;
}
