#ifndef BEMI_VMX_EXIT_ASM_H
#define BEMI_VMX_EXIT_ASM_H

#include <Base.h>

VOID
EFIAPI
BemiVmxExitTrampoline(
  VOID
  );

VOID
EFIAPI
BemiSvmExitTrampoline(
  VOID
  );

EFI_STATUS
EFIAPI
VmxExitDispatch(
  IN UINT64 *GuestGprBase
  );

EFI_STATUS
EFIAPI
SvmExitDispatch(
  IN UINT64 *GuestGprBase,
  IN UINT64 *VmcbBase
  );

#endif
