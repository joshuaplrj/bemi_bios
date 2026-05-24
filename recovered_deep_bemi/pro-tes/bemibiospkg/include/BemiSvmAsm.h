#ifndef BEMI_SVM_ASM_H
#define BEMI_SVM_ASM_H

#include <Base.h>

UINT8
EFIAPI
BemiSvmVmRun(
  IN CONST UINT64 *VmcbPhysicalAddress
  );

UINT8
EFIAPI
BemiSvmVmSave(
  IN CONST UINT64 *VmcbPhysicalAddress
  );

UINT8
EFIAPI
BemiSvmVmLoad(
  IN CONST UINT64 *VmcbPhysicalAddress
  );

UINT8
EFIAPI
BemiSvmStgi(
  VOID
  );

UINT8
EFIAPI
BemiSvmClgi(
  VOID
  );

UINT8
EFIAPI
BemiSvmInvlpga(
  IN UINT64 Address,
  IN UINT32 Asid
  );

#endif
