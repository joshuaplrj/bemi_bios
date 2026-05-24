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
extern EFI_STATUS SmmHandlerInit(UINT64 TraceCacheBase, UINT64 TraceCacheSize);

typedef enum {
  HmcPatternGeneric = 0,
  HmcPatternVideoEncoding = 1,
  HmcPatternOlapScan = 2,
  HmcPatternDlTraining = 3
} HMC_WORKLOAD_PATTERN;

extern EFI_STATUS TraceCacheInit(UINT64 BaseAddress, UINT64 Size);
extern EFI_STATUS TraceCacheConfigureNPP(BOOLEAN EnableNPP, UINT32 HistoryLength, UINT8 *TargetHitRate);
extern EFI_STATUS MemoryCompressorInit(double TargetRatio, double PeakPhysicalBw);
extern EFI_STATUS MemoryCompressorSetWorkloadPattern(HMC_WORKLOAD_PATTERN Pattern);

extern HYPERVISOR_BACKEND gHypervisorBackend;

STATIC EFI_STATUS EFIAPI BemiGetBootMode(IN EFI_BEMI_PROTOCOL *This, OUT UINT8 *BootMode);
STATIC EFI_STATUS EFIAPI BemiSetBootMode(IN EFI_BEMI_PROTOCOL *This, IN UINT8 BootMode);
STATIC EFI_STATUS EFIAPI BemiInitHypervisor(IN EFI_BEMI_PROTOCOL *This, IN UINT64 CpuCount);
STATIC EFI_STATUS EFIAPI BemiHandleCpuid(IN EFI_BEMI_PROTOCOL *This, IN UINT32 Leaf, IN UINT32 Subleaf, OUT UINT32 *Eax, OUT UINT32 *Ebx, OUT UINT32 *Ecx, OUT UINT32 *Edx);

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
  if (BootMode == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  *BootMode = gBootMode;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
BemiSetBootMode(
  IN EFI_BEMI_PROTOCOL  *This,
  IN UINT8               BootMode
  )
{
  if (BootMode >= BemiBootModeMax) {
    return EFI_INVALID_PARAMETER;
  }
  gBootMode = BootMode;

  gRT->SetVariable(
    L"BEMI_NATIVE",
    &gEfiBemiProtocolGuid,
    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
    sizeof(UINT8),
    &gBootMode
  );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
BemiInitHypervisor(
  IN EFI_BEMI_PROTOCOL  *This,
  IN UINT64              CpuCount
  )
{
  EFI_STATUS status;
  DEBUG((DEBUG_INFO, "BEMI: Initializing Hypervisor with %lld CPUs...\n", CpuCount));

  status = HypervisorBackendInit(CpuCount);
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: HypervisorBackendInit failed: %r\n", status));
    return status;
  }

  UINT64 traceCacheSize = PcdGet32(PcdBemiTraceCacheSize);
  UINT8 *traceCacheBuffer = NULL;

  status = gBS->AllocatePool(
    EfiRuntimeServicesData,
    traceCacheSize,
    (VOID **)&traceCacheBuffer
  );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: Failed to allocate trace cache: %r\n", status));
    return status;
  }

  ZeroMem(traceCacheBuffer, traceCacheSize);

  status = SmmHandlerInit((UINT64)traceCacheBuffer, traceCacheSize);
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: SmmHandlerInit failed: %r\n", status));
    gBS->FreePool(traceCacheBuffer);
    return status;
  }

  TraceCacheInit((UINT64)traceCacheBuffer, traceCacheSize);
  TraceCacheConfigureNPP(TRUE, 8, NULL);

  MemoryCompressorInit(2.2, 64.0);
  MemoryCompressorSetWorkloadPattern(HmcPatternDlTraining);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
BemiHandleCpuid(
  IN  EFI_BEMI_PROTOCOL  *This,
  IN  UINT32              Leaf,
  IN  UINT32              Subleaf,
  OUT UINT32             *Eax,
  OUT UINT32             *Ebx,
  OUT UINT32             *Ecx,
  OUT UINT32             *Edx
  )
{
  if (Eax == NULL || Ebx == NULL || Ecx == NULL || Edx == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  CpuidSpoofHandler(Leaf, Subleaf, Eax, Ebx, Ecx, Edx);
  return EFI_SUCCESS;
}

STATIC
VOID
VmxTestGuestLaunch(
  VOID
  )
{
  EFI_STATUS status;
  UINT8      *guestCode = NULL;
  UINT8      *guestStack = NULL;

  DEBUG((DEBUG_INFO, "BEMI: Setting up VMX test guest...\n"));

  status = gBS->AllocatePages(
    AllocateAnyPages,
    EfiRuntimeServicesData,
    1,
    (EFI_PHYSICAL_ADDRESS *)&guestCode
  );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: Failed to allocate guest code page: %r\n", status));
    return;
  }

  status = gBS->AllocatePages(
    AllocateAnyPages,
    EfiRuntimeServicesData,
    1,
    (EFI_PHYSICAL_ADDRESS *)&guestStack
  );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: Failed to allocate guest stack page: %r\n", status));
    gBS->FreePages((EFI_PHYSICAL_ADDRESS)guestCode, 1);
    return;
  }

  guestCode[0] = 0xFA; /* CLI */
  guestCode[1] = 0xF4; /* HLT */

  DEBUG((DEBUG_INFO, "BEMI: Guest code allocated at 0x%llx, stack at 0x%llx\n",
    (UINT64)guestCode, (UINT64)guestStack));

  status = gHypervisorBackend.VcpuCreate(
    0,
    (UINT64)guestCode,
    (UINT64)guestStack + 4096
  );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: VcpuCreate failed: %r\n", status));
    gBS->FreePages((EFI_PHYSICAL_ADDRESS)guestCode, 1);
    gBS->FreePages((EFI_PHYSICAL_ADDRESS)guestStack, 1);
    return;
  }

  DEBUG((DEBUG_INFO, "BEMI: Running test guest...\n"));
  status = gHypervisorBackend.VcpuRun(0);
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: VcpuRun failed: %r\n", status));
  } else {
    DEBUG((DEBUG_INFO, "BEMI: Test guest execution complete\n"));
  }

  gBS->FreePages((EFI_PHYSICAL_ADDRESS)guestCode, 1);
  gBS->FreePages((EFI_PHYSICAL_ADDRESS)guestStack, 1);
}

EFI_STATUS
EFIAPI
BemiBiosEntryPoint(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS status;
  UINTN      variableSize = sizeof(UINT8);
  UINT64     cacheSizes = 0;

  status = PostDetectTopology(&gCpuCount, &cacheSizes);
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
      DEBUG((DEBUG_INFO, "BEMI: Hypervisor init succeeded, launching test guest\n"));
      VmxTestGuestLaunch();
    }
  } else {
    DEBUG((DEBUG_INFO, "BEMI: Mode A (native) — skipping Ring -1 init\n"));
  }

  status = gBS->InstallProtocolInterface(
    &ImageHandle,
    &gEfiBemiProtocolGuid,
    EFI_NATIVE_INTERFACE,
    &gBemiProtocol
  );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "BEMI: Failed to install Bemi protocol: %r\n", status));
    return status;
  }

  DEBUG((DEBUG_INFO, "BEMI: POST complete, ready for OS handoff\n"));
  return EFI_SUCCESS;
}
