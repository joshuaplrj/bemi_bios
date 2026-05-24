#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>

/**
  Bandwidth Governor — v2.0 Scaled Dominance
  =============================================
  Hardware bandwidth monitor that prevents memory bus saturation.

  Problem solved:
    In v1.3, 84 threads at 1.3 IPC could request 80-120 GB/s of memory
    bandwidth, far exceeding the 64 GB/s DDR5 limit. All threads would
    stall simultaneously ("race to stall"), destroying throughput.

  Solution:
    Monitor memory controller transactions in 1000-cycle windows.
    When utilization exceeds 85% of peak (54.4 GB/s), de-schedule 25%
    of active threads (lowest priority). When utilization drops below
    60%, re-enable de-scheduled threads.

  Physical implementation:
    - Performance counter in memory controller (PMC-based)
    - 10-bit saturating counter per 1000-cycle window
    - Signal to thread scheduler: {THROTTLE, NORMAL, BOOST}
    - Latency: 0 cycles (asynchronous monitor, does not gate execution)
**/

#define BW_MONITOR_WINDOW_CYCLES  1000
#define BW_PEAK_GBS               64.0
#define BW_THROTTLE_THRESHOLD      0.85   /* 85% → start throttling */
#define BW_REENABLE_THRESHOLD      0.60   /* 60% → re-enable threads */
#define BW_DESCHEDULE_FRACTION     0.25   /* de-schedule 25% of threads */
#define BW_M
<truncated 58 bytes>
.0,
      gBwGov->ActiveThreads));
  }

  /* Reset window counter */
  gBwGov->TransactionsThisWindow = 0;
}

/**
  Query the current active thread count (used by thread scheduler).

  @retval Active thread count after governor adjustment.
**/
UINT32
BwGovernorGetActiveThreads(
  VOID
  )
{
  if (gBwGov == NULL || !gBwGov->Initialized) {
    return 48;  /* default: V20_THREADS */
  }
  return gBwGov->ActiveThreads;
}

/**
  Query the current governor state.

  @retval BwStateNormal     All threads active.
  @retval BwStateThrottled  25% de-scheduled.
  @retval BwStateSevere     50% de-scheduled.
**/
BW_GOVERNOR_STATE
BwGovernorGetState(
  VOID
  )
{
  if (gBwGov == NULL) return BwStateNormal;
  return gBwGov->State;
}

/**
  Print bandwidth governor statistics.
**/
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
