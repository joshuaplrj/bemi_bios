#ifndef APIC_SHADOW_H
#define APIC_SHADOW_H

#include <Base.h>

VOID
ApicShadowInit(
  IN UINT32 ApicId
  );

UINT32
ApicShadowRead(
  IN UINT32 Offset
  );

VOID
ApicShadowWrite(
  IN UINT32 Offset,
  IN UINT32 Value
  );

VOID
ApicShadowTimerTick(
  VOID
  );

UINT32
ApicShadowInjectInterrupt(
  IN UINT32 Vector
  );

VOID
ApicShadowEoi(
  IN UINT32 Vector
  );

BOOLEAN
ApicGetPendingInterrupt(
  OUT UINT32 *Vector
  );

BOOLEAN
ApicHandleMmioAccess(
  IN UINT64  GuestPhysicalAddress,
  IN BOOLEAN IsWrite,
  IN UINT64  *RegisterValue,
  IN UINT8   AccessSize
  );

VOID
Pic8259Write(
  IN UINT16 Port,
  IN UINT8  Value
  );

UINT8
Pic8259Read(
  IN UINT16 Port
  );

#endif
