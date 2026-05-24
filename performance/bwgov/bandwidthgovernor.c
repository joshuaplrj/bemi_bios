#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>

#define BW_MONITOR_WINDOW_CYCLES  1000
#define BW_PEAK_GBS               64.0
#define BW_THROTTLE_THRESHOLD     0.85   /* 85% → start throttling */
#define BW_REENABLE_THRESHOLD     0.60   /* 60% → re-enable threads */
#define BW_DESCHEDULE_FRACTION    0.25   /* de-schedule 25% of threads */

/* v7.1 DBO-coordinated bandwidth governor defines */
#define V71_MAX_THREADS              84
#define V71_BW_PEAK_GBS              64.0
#define V71_BW_THROTTLE_THRESHOLD    0.80   /* 80% → throttle earlier for 84 threads */
#define V71_BW_REENABLE_THRESHOLD    0.55   /* 55% → re-enable threads */
#define V71_BW_DESCHEDULE_FRACTION   0.20   /* de-schedule 20% of threads, gentler */

typedef enum {
  BwStateNormal,
  BwStateThrottled,
  BwStateSevere
} BW_GOVERNOR_STATE;

typedef struct {
  BOOLEAN           Initialized;
  BW_GOVERNOR_STATE State;
  UINT32            MaxThreads;
  UINT32            ActiveThreads;
  UINT64            TransactionsThisWindow;
  UINT64            TotalTransactions;
  UINT64            WindowCount;
  double            CurrentBwGbs;
  double            AverageBwGbs;
  double            PeakBwGbs;
  UINT64            ThrottleEvents;
  UINT64            ReenableEvents;

  /* v7.1 DBO-prefetch tracking */
  UINT64            DboPrefetchRequests;
  UINT64            DboPrefetchHits;
} BW_GOVERNOR;

STATIC BW_GOVERNOR *gBwGov = NULL;

EFI_STATUS
BwGovernorInit(
  IN UINT32 MaxThreads,
  IN double PeakBwGbs
  )
{
  gBwGov = (BW_GOVERNOR *)AllocateZeroPool(sizeof(BW_GOVERNOR));
  if (gBwGov == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  gBwGov->MaxThreads = MaxThreads;
  gBwGov->ActiveThreads = MaxThreads;
  gBwGov->State = BwStateNormal;
  gBwGov->PeakBwGbs = (PeakBwGbs > 0.0) ? PeakBwGbs : BW_PEAK_GBS;
  gBwGov->Initialized = TRUE;

  DEBUG((DEBUG_INFO, "BW_GOV: Initialized (MaxThreads=%d, PeakBw=%.1f GB/s)\n",
    MaxThreads, gBwGov->PeakBwGbs));

  return EFI_SUCCESS;
}

EFI_STATUS
BwGovernorV71Init(
  VOID
  )
{
  return BwGovernorInit(V71_MAX_THREADS, V71_BW_PEAK_GBS);
}

VOID
BwGovernorRegisterTransaction(
  IN UINT32 Count
  )
{
  if (gBwGov == NULL || !gBwGov->Initialized) return;

  gBwGov->TransactionsThisWindow += Count;
  gBwGov->TotalTransactions += Count;
}

VOID
BwGovernorV71RegisterDboPrefetch(
  IN UINT32 Requests,
  IN UINT32 Hits
  )
{
  if (gBwGov == NULL || !gBwGov->Initialized) return;

  gBwGov->DboPrefetchRequests += Requests;
  gBwGov->DboPrefetchHits += Hits;
}

VOID
BwGovernorProcessWindow(
  VOID
  )
{
  if (gBwGov == NULL || !gBwGov->Initialized) return;

  gBwGov->WindowCount++;

  double windowBw = (double)gBwGov->TransactionsThisWindow * 0.15;
  gBwGov->CurrentBwGbs = windowBw;

  gBwGov->AverageBwGbs = (gBwGov->AverageBwGbs * 0.9) + (windowBw * 0.1);

  double utilization = gBwGov->CurrentBwGbs / gBwGov->PeakBwGbs;

  BW_GOVERNOR_STATE oldState = gBwGov->State;

  /* v7.1 DBO-coordinated bandwidth governance: throttle/re-enable with hysteresis */
  if (utilization >= BW_THROTTLE_THRESHOLD) {
    if (gBwGov->State == BwStateNormal) {
      gBwGov->State = BwStateThrottled;
      gBwGov->ActiveThreads = (UINT32)(gBwGov->MaxThreads * (1.0 - BW_DESCHEDULE_FRACTION));
      gBwGov->ThrottleEvents++;
    } else if (gBwGov->State == BwStateThrottled) {
      gBwGov->State = BwStateSevere;
      gBwGov->ActiveThreads = gBwGov->MaxThreads / 2;
      gBwGov->ThrottleEvents++;
    }
  } else if (utilization <= BW_REENABLE_THRESHOLD) {
    if (gBwGov->State == BwStateSevere) {
      gBwGov->State = BwStateThrottled;
      gBwGov->ActiveThreads = (UINT32)(gBwGov->MaxThreads * (1.0 - BW_DESCHEDULE_FRACTION));
      gBwGov->ReenableEvents++;
    } else if (gBwGov->State == BwStateThrottled) {
      gBwGov->State = BwStateNormal;
      gBwGov->ActiveThreads = gBwGov->MaxThreads;
      gBwGov->ReenableEvents++;
    }
  }

  if (gBwGov->State != oldState) {
    DEBUG((DEBUG_INFO, "BW_GOV: State transition %d -> %d. Active threads: %d\n",
      oldState, gBwGov->State,
      gBwGov->ActiveThreads));
  }

  /* Reset window counter */
  gBwGov->TransactionsThisWindow = 0;
}

UINT32
BwGovernorGetActiveThreads(
  VOID
  )
{
  if (gBwGov == NULL || !gBwGov->Initialized) {
    return V71_MAX_THREADS;  /* default for v7.1 DBO-coordinated governor */
  }
  return gBwGov->ActiveThreads;
}

UINT32
BwGovernorV71GetEffectiveThreads(
  VOID
  )
{
  if (gBwGov == NULL || !gBwGov->Initialized) {
    return V71_MAX_THREADS;
  }

  if (gBwGov->State == BwStateNormal) {
    return gBwGov->MaxThreads;
  }

  return gBwGov->ActiveThreads;
}

BW_GOVERNOR_STATE
BwGovernorGetState(
  VOID
  )
{
  if (gBwGov == NULL) return BwStateNormal;
  return gBwGov->State;
}

VOID
BwGovernorPrintStats(
  VOID
  )
{
  if (gBwGov == NULL) return;

  DEBUG((DEBUG_INFO, "BW_GOV Stats:\n"));
  DEBUG((DEBUG_INFO, "  Total transactions : %lld\n", gBwGov->TotalTransactions));
  DEBUG((DEBUG_INFO, "  Monitoring windows : %lld\n", gBwGov->WindowCount));
  DEBUG((DEBUG_INFO, "  Current BW         : %.1f GB/s (%.0f%% of %.0f GB/s peak)\n",
    gBwGov->CurrentBwGbs,
    (gBwGov->PeakBwGbs > 0) ? gBwGov->CurrentBwGbs / gBwGov->PeakBwGbs * 100.0 : 0.0,
    gBwGov->PeakBwGbs));
  DEBUG((DEBUG_INFO, "  Average BW         : %.1f GB/s\n", gBwGov->AverageBwGbs));
  DEBUG((DEBUG_INFO, "  Active threads     : %d / %d (state=%d)\n",
    gBwGov->ActiveThreads, gBwGov->MaxThreads, gBwGov->State));
  DEBUG((DEBUG_INFO, "  Throttle events    : %lld\n", gBwGov->ThrottleEvents));
  DEBUG((DEBUG_INFO, "  Re-enable events   : %lld\n", gBwGov->ReenableEvents));
}
