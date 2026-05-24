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
#define B
// missing line 36
// missing line 37
// missing line 38
// missing line 39
// missing line 40
// missing line 41
// missing line 42
// missing line 43
// missing line 44
// missing line 45
// missing line 46
// missing line 47
// missing line 48
// missing line 49
// missing line 50
// missing line 51
// missing line 52
// missing line 53
// missing line 54
// missing line 55
// missing line 56
// missing line 57
// missing line 58
// missing line 59
// missing line 60
// missing line 61
// missing line 62
// missing line 63
// missing line 64
// missing line 65
// missing line 66
// missing line 67
// missing line 68
// missing line 69
// missing line 70
// missing line 71
// missing line 72
// missing line 73
// missing line 74
// missing line 75
// missing line 76
// missing line 77
// missing line 78
// missing line 79
// missing line 80
// missing line 81
// missing line 82
// missing line 83
// missing line 84
// missing line 85
// missing line 86
// missing line 87
// missing line 88
// missing line 89
// missing line 90
// missing line 91
// missing line 92
// missing line 93
// missing line 94
// missing line 95
// missing line 96
// missing line 97
// missing line 98
// missing line 99
// missing line 100
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.