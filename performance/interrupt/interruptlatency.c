#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>

#define MAX_INTERRUPT_VECTORS 256
#define POSTED_INTERRUPT_VECTOR 0xFE
#define INTERRUPT_COALESCE_WINDOW 100

typedef struct {
  UINT8   Vector;
  UINT64  ArrivalTime;
  UINT64  DeliveryTime;
  BOOLEAN Pending;
  BOOLEAN Coalesced;
} INTERRUPT_EVENT;

typedef struct {
  UINT8   Vector;
  UINT64  HandlerAddress;
  BOOLEAN Registered;
  UINT64  TotalLatency;
  UINT64  MaxLatency;
  UINT64  Count;
} INTERRUPT_HANDLER;

STATIC INTERRUPT_EVENT  gInterruptEvents[MAX_INTERRUPT_VECTORS];
STATIC INTERRUPT_HANDLER gInterruptHandlers[MAX_INTERRUPT_VECTORS];
STATIC UINT64 gMaxObservedLatency = 0;
STATIC UINT64 gTotalInterrupts = 0;
STATIC UINT64 gPostedInterrupts = 0;
STATIC UINT64 gCoalescedInterrupts = 0;
STATIC UINT64 gFastPathInterrupts = 0;

VOID
InterruptLatencyInit(
  VOID
  )
{
  ZeroMem(gInterruptEvents, sizeof(gInterruptEvents));
  ZeroMem(gInterruptHandlers, sizeof(gInterruptHandlers));
  gMaxObservedLatency = 0;
  gTotalInterrupts = 0;
  gPostedInterrupts = 0;
  gCoalescedInterrupts = 0;
  gFastPathInterrupts = 0;

  DEBUG((DEBUG_INFO, "INTERRUPT: Latency framework initialized\n"));
}

UINT64
InterruptLatencyMeasure(
  VOID
  )
{
  return AsmReadTsc();
}

EFI_STATUS
InterruptRegisterHandler(
  IN UINT8 Vector,
  IN UINT64 HandlerAddress
  )
{
  if (Vector >= MAX_INTERRUPT_VECTORS) {
    return EFI_INVALID_PARAMETER;
  }
  gInterruptHandlers[Vector].Vector = Vector;
  gInterruptHandlers[Vector].HandlerAddress = HandlerAddress;
  gInterruptHandlers[Vector].Registered = TRUE;
  return EFI_SUCCESS;
}

VOID
InterruptTrigger(
  IN UINT8 Vector
  )
{
  UINT64 arrivalTime = InterruptLatencyMeasure();
  gTotalInterrupts++;

  // Check if we can coalesce this interrupt
  for (UINT32 i = 0; i < MAX_INTERRUPT_VECTORS; i++) {
    if (gInterruptEvents[i].Pending && gInterruptEvents[i].Vector == Vector) {
      if (arrivalTime - gInterruptEvents[i].ArrivalTime < INTERRUPT_COALESCE_WINDOW) {
        gInterruptEvents[i].Coalesced = TRUE;
        gCoalescedInterrupts++;
        return;
      }
    }
  }

  // Find a free slot in our interrupt event queue
  for (UINT32 i = 0; i < MAX_INTERRUPT_VECTORS; i++) {
    if (!gInterruptEvents[i].Pending) {
      gInterruptEvents[i].Vector = Vector;
      gInterruptEvents[i].ArrivalTime = arrivalTime;
      gInterruptEvents[i].Pending = TRUE;
      gInterruptEvents[i].Coalesced = FALSE;
      break;
    }
  }
}

VOID
InterruptDeliver(
  IN UINT8 Vector
  )
{
  UINT64 deliveryTime = InterruptLatencyMeasure();
  UINT64 latencyUs = 0;

  for (UINT32 i = 0; i < MAX_INTERRUPT_VECTORS; i++) {
    if (gInterruptEvents[i].Pending &&
        gInterruptEvents[i].Vector == Vector) {
      gInterruptEvents[i].DeliveryTime = deliveryTime;
      gInterruptEvents[i].Pending = FALSE;

      latencyUs = (deliveryTime - gInterruptEvents[i].ArrivalTime);

      if (latencyUs > gMaxObservedLatency) {
        gMaxObservedLatency = latencyUs;
      }

      if (gInterruptHandlers[Vector].Registered) {
        gInterruptHandlers[Vector].TotalLatency += latencyUs;
        gInterruptHandlers[Vector].Count++;
        if (latencyUs > gInterruptHandlers[Vector].MaxLatency) {
          gInterruptHandlers[Vector].MaxLatency = latencyUs;
        }
      }
      break;
    }
  }

  if (latencyUs < 5) {
    gFastPathInterrupts++;
  }
}

BOOLEAN
InterruptUsePosted(
  IN UINT8 Vector
  )
{
  if (Vector == POSTED_INTERRUPT_VECTOR) {
    gPostedInterrupts++;
    return TRUE;
  }
  return FALSE;
}

BOOLEAN
InterruptUseFastPath(
  IN UINT8 Vector,
  IN UINT64 CurrentLatency
  )
{
  if (CurrentLatency < 1000) {
    return TRUE;
  }
  return (Vector < 32);
}

VOID
InterruptGetStats(
  OUT UINT64 *Total,
  OUT UINT64 *MaxLatency,
  OUT UINT64 *PostedCount,
  OUT UINT64 *CoalescedCount,
  OUT UINT64 *FastPathCount
  )
{
  *Total = gTotalInterrupts;
  *MaxLatency = gMaxObservedLatency;
  *PostedCount = gPostedInterrupts;
  *CoalescedCount = gCoalescedInterrupts;
  *FastPathCount = gFastPathInterrupts;
}
