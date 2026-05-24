#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Bemicpuid.h>
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

/* Hypervisor leaf signature: "BEMI BEMI BEMI" */
#define BEMI_HV_SIGNATURE_EBX  0x494d4542   /* "BEMI" */
#define BEMI_HV_SIGNATURE_ECX  0x494d4542   /* "BEMI" */
#define BEMI_HV_SIGNATURE_EDX  0x494d4542   /* "BEMI" */
#define BEMI_HV_MAX_LEAF       0x40000005

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
    gCpuidOverrides[gCpuidOverrideCount].Leaf    = Leaf;
    gCpuidOverrides[gCpuidOverrideCount].Subleaf = Subleaf;
    gCpuidOverrides[gCpuidOverrideCount].Eax     = Eax;
    gCpuidOverrides[gCpuidOverrideCount].Ebx     = Ebx;
    gCpuidOverrides[gCpuidOverrideCount].Ecx     = Ecx;
    gCpuidOverrides[gCpuidOverrideCount].Edx     = Edx;
    gCpuidOverrideCount++;
  }
}

/**
  Leaf 01h - Feature flags.
  Force expose VMX (ECX bit 5) and mask out AES/PCLMULQDQ if AMD.
**/
STATIC
VOID
PopulateLeaf01(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x01, &eax, &ebx, &ecx, &edx);

  /* Force VMX support bit */
  ecx |= BIT5;
  /* Force Hypervisor Present bit (bit 31) */
  ecx |= BIT31;
  /* Mask PDCM (bit 15) to avoid PMU conflicts */
  ecx &= ~BIT15;

  AddOverride(0x01, 0, eax, ebx, ecx, edx);
}

/**
  Leaf 04h - Cache topology.
**/
STATIC
VOID
PopulateLeaf04(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  for (UINT32 subleaf = 0; subleaf < 5; subleaf++) {
    AsmCpuidEx(0x04, subleaf, &eax, &ebx, &ecx, &edx);
    UINT32 cacheType = eax & 0x1F;
    if (cacheType == 0) {
      if (subleaf == 3) {
        eax = 3 | (3 << 5) | BIT8;
        ebx = (15 << 22) | (0 << 12) | 63;
        ecx = 16383;
        edx = 0;
        AddOverride(0x04, subleaf, eax, ebx, ecx, edx);
      }
      break;
    }
    if (cacheType == 3) {
      eax = (eax & ~0x1F) | 3;
      eax |= (3 << 5);
      ebx = (15 << 22) | (0 << 12) | 63;
      ecx = 16383;
    }
    AddOverride(0x04, subleaf, eax, ebx, ecx, edx);
  }
}

/**
  Leaf 07h - Structured Extended Features.
**/
STATIC
VOID
PopulateLeaf07(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuidEx(0x07, 0, &eax, &ebx, &ecx, &edx);
  AddOverride(0x07, 0, eax, ebx, ecx, edx);
}

/**
  Leaf 0Bh - Extended Topology.
**/
STATIC
VOID
PopulateLeaf0B(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  for (UINT32 level = 0; level < 4; level++) {
    AsmCpuidEx(0x0B, level, &eax, &ebx, &ecx, &edx);
    UINT32 type = (ecx >> 8) & 0xFF;
    AddOverride(0x0B, level, eax, ebx, ecx, edx);
    if (type == 0) {
      break;
    }
  }
}

/**
  Leaf 0Dh - Processor Extended State.
**/
STATIC
VOID
PopulateLeaf0D(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  for (UINT32 subleaf = 0; subleaf < 10; subleaf++) {
    AsmCpuidEx(0x0D, subleaf, &eax, &ebx, &ecx, &edx);
    if (subleaf > 0 && eax == 0 && ebx == 0 && ecx == 0 && edx == 0) {
      break;
    }
    AddOverride(0x0D, subleaf, eax, ebx, ecx, edx);
  }
}

/**
  Hypervisor Specific Leaves.
**/
STATIC
VOID
PopulateHypervisorLeaves(
  VOID
  )
{
  AddOverride(0x40000000, 0, BEMI_HV_MAX_LEAF, BEMI_HV_SIGNATURE_EBX, BEMI_HV_SIGNATURE_ECX, BEMI_HV_SIGNATURE_EDX);
  AddOverride(0x40000001, 0, 0x00010003, 0, 0, 0);
  AddOverride(0x40000002, 0, 0, 0, 0, 0);
  AddOverride(0x40000003, 0, 0, 0, 0, 0);
  AddOverride(0x40000004, 0, 0, 0, 0, 0);
  AddOverride(0x40000005, 0, 0, 0, 0, 0);
}

/**
  Extended Leaves.
**/
STATIC
VOID
PopulateExtendedLeaves(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x80000000, &eax, &ebx, &ecx, &edx);
  gMaxExtendedLeaf = eax;

  for (UINT32 leaf = 0x80000000; leaf <= gMaxExtendedLeaf; leaf++) {
    AsmCpuid(leaf, &eax, &ebx, &ecx, &edx);
    if (leaf == 0x80000001) {
      ecx |= BIT2; /* Expose SVM support */
    }
    AddOverride(leaf, 0, eax, ebx, ecx, edx);
  }
}

/**
  Other leaves.
**/
STATIC
VOID
PopulateOtherLeaves(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  for (UINT32 leaf = 0x02; leaf <= gMaxStandardLeaf; leaf++) {
    if (leaf == 0x04 || leaf == 0x07 || leaf == 0x0B || leaf == 0x0D) {
      continue;
    }
    AsmCpuid(leaf, &eax, &ebx, &ecx, &edx);
    AddOverride(leaf, 0, eax, ebx, ecx, edx);
  }
}

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
