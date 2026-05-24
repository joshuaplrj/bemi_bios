#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <BemiCpuid.h>
#include <BemiApi.h>

typedef struct {
  UINT32 Leaf;
  UINT32 Subleaf;
  UINT32 Eax;
  UINT32 Ebx;
  UINT32 Ecx;
  UINT32 Edx;
} CPUID_OVERRIDE;

#define MAX_CPUID_OVERRIDES 128
#define IS_INTEL(v) (AsciiStrCmp((v), "GenuineIntel") == 0)
#define IS_AMD(v)   (AsciiStrCmp((v), "AuthenticAMD") == 0)

STATIC CPUID_OVERRIDE gCpuidOverrides[MAX_CPUID_OVERRIDES];
STATIC UINT32 gCpuidOverrideCount = 0;
STATIC CHAR8 gVendorString[13];
STATIC BOOLEAN gIsIntel = FALSE;
STATIC BOOLEAN gIsAmd = FALSE;
STATIC UINT32 gMaxStandardLeaf = 0;
STATIC UINT32 gMaxExtendedLeaf = 0;

STATIC VOID
AddOverride(
  UINT32 Leaf,
  UINT32 Subleaf,
  UINT32 Eax,
  UINT32 Ebx,
  UINT32 Ecx,
  UINT32 Edx
  )
{
  if (gCpuidOverrideCount < MAX_CPUID_OVERRIDES) {
    gCpuidOverrides[gCpuidOverrideCount].Leaf = Leaf;
    gCpuidOverrides[gCpuidOverrideCount].Subleaf = Subleaf;
    gCpuidOverrides[gCpuidOverrideCount].Eax = Eax;
    gCpuidOverrides[gCpuidOverrideCount].Ebx = Ebx;
    gCpuidOverrides[gCpuidOverrideCount].Ecx = Ecx;
    gCpuidOverrides[gCpuidOverrideCount].Edx = Edx;
    gCpuidOverrideCount++;
  }
}

STATIC
VOID
PopulateLeaf01(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x01, &eax, &ebx, &ecx, &edx);
58
<truncated 6805 bytes>
VOID
CpuidSpoofInit(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;

  AsmCpuid(0x00, &eax, &ebx, &ecx, &edx);
  *(UINT32 *)&gVendorString[0] = ebx;
  *(UINT32 *)&gVendorString[4] = edx;
  *(UINT32 *)&gVendorString[8] = ecx;
  gVendorString[12] = '\0';

  gIsIntel = IS_INTEL(gVendorString);
  gIsAmd = IS_AMD(gVendorString);
  gMaxStandardLeaf = eax;

  gCpuidOverrideCount = 0;

  PopulateLeaf01();
  PopulateLeaf04();
  PopulateLeaf07();
  PopulateLeaf0B();
  PopulateLeaf0D();
  PopulateHypervisorLeaves();
  PopulateExtendedLeaves();
  PopulateOtherLeaves();

  DEBUG((DEBUG_INFO, "CPUID: Spoof initialized for %s. %d overrides (max leaf 0x%X, ext 0x%X)\n",
    gVendorString, gCpuidOverrideCount, gMaxStandardLeaf, gMaxExtendedLeaf));
}

VOID
CpuidSpoofHandler(
  IN  UINT32 Leaf,
  IN  UINT32 Subleaf,
  OUT UINT32 *Eax,
  OUT UINT32 *Ebx,
  OUT UINT32 *Ecx,
  OUT UINT32 *Edx
  )
{
  for (UINT32 i = 0; i < gCpuidOverrideCount; i++) {
    if (gCpuidOverrides[i].Leaf == Leaf && gCpuidOverrides[i].Subleaf == Subleaf) {
      *Eax = gCpuidOverrides[i].Eax;
      *Ebx = gCpuidOverrides[i].Ebx;
      *Ecx = gCpuidOverrides[i].Ecx;
      *Edx = gCpuidOverrides[i].Edx;
      return;
    }
  }

  AsmCpuidEx(Leaf, Subleaf, Eax, Ebx, Ecx, Edx);

  if (Leaf >= 0x40000001 && Leaf <= 0x4FFFFFFF) {
    *Eax = 0;
    *Ebx = 0;
    *Ecx = 0;
    *Edx = 0;
  }

  if (Leaf >= 0x80000009 && Leaf <= 0x80000018 && Leaf != 0x8000000A &&
      Leaf != 0x80000019 && Leaf != 0x8000001A &&
      Leaf != 0x8000001D && Leaf != 0x8000001E && Leaf != 0x8000001F) {
    *Eax = 0;
    *Ebx = 0;
    *Ecx = 0;
    *Edx = 0;
  }
}
