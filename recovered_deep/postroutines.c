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
        UINT32 lineSize = (ebx
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.