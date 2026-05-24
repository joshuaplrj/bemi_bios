#ifndef BEMI_VMX_ASM_H
#define BEMI_VMX_ASM_H

#include <Base.h>

UINT8
EFIAPI
BemiVmxOn(
  IN CONST UINT64 *VmxonRegionPhysicalAddress
  );

UINT8
EFIAPI
BemiVmxOff(
  VOID
  );

UINT8
EFIAPI
BemiVmClear(
  IN CONST UINT64 *VmcsPhysicalAddress
  );

UINT8
EFIAPI
BemiVmPtrLd(
  IN CONST UINT64 *VmcsPhysicalAddress
  );

UINT8
EFIAPI
BemiVmRead(
  IN  UINT64 Field,
  OUT UINT64 *Value
  );

UINT8
EFIAPI
BemiVmWrite(
  IN UINT64 Field,
  IN UINT64 Value
  );

UINT8
EFIAPI
BemiVmxLaunch(
  VOID
  );

UINT8
EFIAPI
BemiVmxResume(
  VOID
  );

#endif
