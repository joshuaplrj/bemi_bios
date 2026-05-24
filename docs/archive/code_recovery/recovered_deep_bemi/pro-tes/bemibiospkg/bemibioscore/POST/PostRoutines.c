#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_STATUS
PostDetectTopology(
  OUT UINT64 *CpuCount,
  OUT UINT64 *CacheSizes
  )
{
  UINT32 eax, ebx, ecx, edx;
  UINT32 cores = 0;
  UINT32 threadsPerCore = 1;
  UINT32 cacheSizeL3 = 0;

  AsmCpuid(0x00, &eax, &ebx, &ecx, &edx);
  UINT32 maxLeaf = eax;

  if (maxLeaf >= 0x0B) {
    for (UINT32 level = 0; level < 2; level++) {
      AsmCpuidEx(0x0B, level, &eax, &ebx, &ecx, &edx);
      UINT32 type = (ecx >> 8) & 0xFF;
      if (type == 1) {
        threadsPerCore = ebx & 0xFFFF;
      } else if (type == 2) {
        cores = ebx & 0xFFFF;
      }
    }
  } else if (maxLeaf >= 0x01) {
    AsmCpuid(0x01, &eax, &ebx, &ecx, &edx);
    UINT8 htBits = (ebx >> 16) & 0xFF;
    threadsPerCore = htBits;
  }

  if (cores == 0) cores = threadsPerCore;

  if (maxLeaf >= 0x04) {
    for (UINT32 cacheLevel = 0; cacheLevel < 10; cacheLevel++) {
      AsmCpuidEx(0x04, cacheLevel, &eax, &ebx, &ecx, &edx);
      UINT32 cacheType = eax & 0x1F;
      if (cacheType == 0) break;
      if (cacheType == 3) {
        UINT32 ways = ((ebx >> 22) & 0x3FF) + 1;
        UINT32 partitions = ((ebx >> 12) & 0x3FF) + 1;
        UINT32 lineSize = (ebx & 0xFFF) + 1;
        UINT32 sets 
<truncated 534 bytes>
{
    UINT64 featureControl = AsmReadMsr64(0x3A);
    *Supported = ((featureControl & BIT0) != 0) && ((featureControl & BIT2) != 0);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
PostValidateSvmSupport(
  OUT BOOLEAN *Supported
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x80000001, &eax, &ebx, &ecx, &edx);

  *Supported = (ecx & BIT2) != 0;

  return EFI_SUCCESS;
}

EFI_STATUS
PostCalculateThreadCount(
  IN  UINT64 CpuCount,
  IN  UINT64 CacheSizes,
  OUT UINT64 *BemiThreads
  )
{
  UINT64 baseThreads = CpuCount * 2;

  UINT64 l3Mb = CacheSizes / (1024 * 1024);
  if (l3Mb >= 16) {
    baseThreads = CpuCount * 4;
  } else if (l3Mb >= 8) {
    baseThreads = CpuCount * 3;
  } else {
    baseThreads = CpuCount * 2;
  }

  if (baseThreads > 84) baseThreads = 84;
  if (baseThreads < 1) baseThreads = 1;

  *BemiThreads = baseThreads;
  return EFI_SUCCESS;
}

EFI_STATUS
PostExecutePowerOnSelfTest(
  VOID
  )
{
  UINT64 *testBuffer;
  UINTN wordCount = 256;

  DEBUG((DEBUG_INFO, "POST: Validating low memory...\n"));
  testBuffer = AllocateZeroPool(wordCount * sizeof(UINT64));
  if (testBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (UINTN i = 0; i < wordCount; i++) {
    testBuffer[i] = 0xDEADBEEFCAFEBABE;
  }

  for (UINTN i = 0; i < wordCount; i++) {
    if (testBuffer[i] != 0xDEADBEEFCAFEBABE) {
      DEBUG((DEBUG_ERROR, "POST: Memory test failed at offset 0x%llx\n", (UINT64)i * 8));
      FreePool(testBuffer);
      return EFI_DEVICE_ERROR;
    }
  }

  FreePool(testBuffer);

  IoWrite8(0x80, 0x01);
  DEBUG((DEBUG_INFO, "POST: Complete OK\n"));
  return EFI_SUCCESS;
}
