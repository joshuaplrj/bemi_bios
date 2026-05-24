#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/PcdLib.h>
#include <Protocol/BemiProtocol.h>
#include <BemiCpuid.h>
#include "BemiApi.h"

extern EFI_STATUS PostDetectTopology(UINT64 *CpuCount, UINT64 *CacheSizes);
extern EFI_STATUS PostValidateVmxSupport(BOOLEAN *Supported);
extern EFI_STATUS PostValidateSvmSupport(BOOLEAN *Supported);
extern EFI_STATUS PostCalculateThreadCount(UINT64 CpuCount, UINT64 CacheSizes, UINT64 *BemiThreads);

extern EFI_STATUS AcpiInitTables(UINT32 CpuCount);
extern EFI_STATUS SmbiosInitTables(UINT32 CpuCount);

extern EFI_STATUS HypervisorBackendInit(UINT64 CpuCount);

extern HYPERVISOR_BACKEND gHypervisorBackend;

EFI_BEMI_PROTOCOL gBemiProtocol = {
  BemiGetBootMode,
  BemiSetBootMode,
  BemiInitHypervisor,
  BemiHandleCpuid
};

UINT8  gBootMode = 0;
UINT64 gCpuCount = 0;
UINT64 gBemiThreadCount = 84;

STATIC
EFI_STATUS
EFIAPI
BemiGetBootMode(
  IN  EFI_BEMI_PROTOCOL *This,
  OUT UINT8             *BootMode
  )
{
  *BootMode = gBootMode;
  return EFI_SUCCESS;
}
  IoLib
  CacheMaintenanceLib
  SynchronizationLib
  MemoryAllocationLib
  PcdLib
  DevicePathLib
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_WARN, "BEMI: Topology detection failed, defaulting to 1 CPU\n"));
    gCpuCount = 1;
  }

  status = PostCalculateThreadCount(gCpuCount, cacheSizes, &gBemiThreadCount);
  if (EFI_ERROR(status)) {
    gBemiThreadCount = 84;
  }

  DEBUG((DEBUG_INFO, "BEMI: Detected %lld CPUs, BEMI v1.3 threads: %lld\n", gCpuCount, gBemiThreadCount));

  status = gRT->GetVariable(
    L"BEMI_NATIVE",
    &gEfiBemiProtocolGuid,
    NULL,
    &variableSize,
    &gBootMode
  );
  if (EFI_ERROR(status)) {
    gBootMode = PcdGet8(PcdBemiBootMode);
    gRT->SetVariable(
      L"BEMI_NATIVE",
      &gEfiBemiProtocolGuid,
      EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
      sizeof(UINT8),
      &gBootMode
    );
  }

  CpuidSpoofInit();

  AcpiInitTables((UINT32)gCpuCount);
  SmbiosInitTables((UINT32)gCpuCount);

  if (gBootMode == BemiBootModeLegacy) {
    DEBUG((DEBUG_INFO, "BEMI: Mode B (legacy) — initializing Ring -1 hypervisor\n"));
    status = BemiInitHypervisor(&gBemiProtocol, gCpuCount);
    if (EFI_ERROR(status)) {
      if (status == EFI_UNSUPPORTED) {
        DEBUG((DEBUG_WARN, "BEMI: Hypervisor init not enabled yet: %r\n", status));
      } else {
        DEBUG((DEBUG_ERROR, "BEMI: Hypervisor init failed: %r\n", status));
      }
    } else {
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.