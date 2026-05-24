#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <BemiCpuid.h>
#include "HypervisorBackend.h"

HYPERVISOR_BACKEND gHypervisorBackend;

extern EFI_STATUS VmxInit(UINT64 CpuCount);
extern EFI_STATUS VmxCreateVcpu(UINT64 VcpuId, UINT64 EntryPoint, UINT64 StackPointer);
extern EFI_STATUS VmxRunVcpu(UINT64 VcpuId);

extern EFI_STATUS SvmInit(UINT64 CpuCount);
extern EFI_STATUS SvmCreateVcpu(UINT64 VcpuId, UINT64 EntryPoint, UINT64 StackPointer);
extern EFI_STATUS SvmRunVcpu(UINT64 VcpuId);

BOOLEAN VmxDetect(VOID);
BOOLEAN SvmDetect(VOID);

EFI_STATUS
HypervisorDetectVendor(
  OUT HYPERVISOR_VENDOR *Vendor
  )
{
  UINT32 eax, ebx, ecx, edx;
  CHAR8 vendor[13];

  AsmCpuid(0x00, &eax, &ebx, &ecx, &edx);
  *(UINT32 *)&vendor[0] = ebx;
  *(UINT32 *)&vendor[4] = edx;
  *(UINT32 *)&vendor[8] = ecx;
  vendor[12] = '\0';

  if (AsciiStrCmp(vendor, "GenuineIntel") == 0) {
    *Vendor = HypervisorIntel;
  } else if (AsciiStrCmp(vendor, "AuthenticAMD") == 0) {
    *Vendor = HypervisorAmd;
  } else {
    *Vendor = HypervisorUnknown;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
HypervisorBackendInit(
  IN UINT64 CpuCount
  )
{
  HYPERVISOR_VENDOR vendor;

  ZeroMem(&gHyperv
<truncated 97 bytes>
  )
{
  UINT64 crNumber = ExitInfo->ExitQualification & 0x0F;
  UINT64 accessType = (ExitInfo->ExitQualification >> 4) & 0x03;

  if (accessType == 0) {
    switch (crNumber) {
      case 0: GuestState->GuestRax = GuestState->GuestCr0; break;
      case 3: GuestState->GuestRax = GuestState->GuestCr3; break;
      case 4: GuestState->GuestRax = GuestState->GuestCr4; break;
    }
  } else {
    UINT64 newValue = GuestState->GuestRax;
    switch (crNumber) {
      case 0: GuestState->GuestCr0 = newValue; break;
      case 3:
        GuestState->GuestCr3 = newValue;
        break;
      case 4: GuestState->GuestCr4 = newValue; break;
    }
  }

  GuestState->GuestRip += ExitInfo->InstructionLength;
  return EFI_SUCCESS;
}

EFI_STATUS
HandleIoExit(
  IN VM_EXIT_INFO *ExitInfo,
  IN OUT GUEST_STATE *GuestState
  )
{
  UINT16 port = (UINT16)(ExitInfo->ExitQualification >> 16) & 0xFFFF;
  BOOLEAN isWrite = (ExitInfo->ExitQualification & 0x08) == 0;
  UINT8 size = (UINT8)(ExitInfo->ExitQualification & 0x07);

  if (port == 0x3F8) {
    if (isWrite) {
      IoWrite8(port, (UINT8)(GuestState->GuestRax & 0xFF));
    } else {
      GuestState->GuestRax = IoRead8(port);
    }
    GuestState->GuestRip += ExitInfo->InstructionLength;
    return EFI_SUCCESS;
  }

  switch (size) {
    case 0: if (!isWrite) GuestState->GuestRax = (GuestState->GuestRax & ~0xFF) | (IoRead8(port) & 0xFF); break;
    case 1: if (!isWrite) GuestState->GuestRax = (GuestState->GuestRax & ~0xFFFF) | (IoRead16(port) & 0xFFFF); break;
    case 2: if (!isWrite) GuestState->GuestRax = IoRead32(port); break;
  }

  GuestState->GuestRip += ExitInfo->InstructionLength;
  return EFI_SUCCESS;
}
