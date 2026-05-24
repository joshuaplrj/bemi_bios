#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

#define MAX_SHADOW_MSRS 256

typedef struct {
  UINT32  Index;
  UINT64  Value;
  BOOLEAN IsShadowed;
  BOOLEAN IsReadOnly;
  BOOLEAN PassthroughOnWrite;
} MSR_SHADOW_ENTRY;

STATIC MSR_SHADOW_ENTRY gMsrShadowTable[MAX_SHADOW_MSRS];
STATIC UINT32 gMsrShadowCount = 0;

#define MSR_IA32_TSC            0x00000010
#define MSR_IA32_APIC_BASE      0x0000001B
#define MSR_IA32_TSC_ADJUST     0x0000003B
#define MSR_IA32_MTRR_CAP       0x000000FE
#define MSR_IA32_SYSENTER_CS    0x00000174
#define MSR_IA32_SYSENTER_ESP   0x00000175
#define MSR_IA32_SYSENTER_EIP   0x00000176
#define MSR_IA32_MCG_CAP        0x00000179
#define MSR_IA32_MCG_STATUS     0x0000017A
#define MSR_IA32_MCG_CTL        0x0000017B
#define MSR_IA32_PAT            0x00000277
#define MSR_IA32_MTRR_DEF_TYPE  0x000002FF

#define MSR_IA32_MTRR_PHYSBASE0 0x00000200
#define MSR_IA32_MTRR_PHYSMASK0 0x00000201
#define MSR_IA32_MTRR_PHYSBASE1 0x00000202
#define MSR_IA32_MTRR_PHYSMASK1 0x00000203
#define MSR_IA32_MTRR_PHYSBASE2 0x00000204
#define MSR_IA32_MTRR_PHYSMASK2 0x00000205
#define MSR_IA32_MTRR_PHYSBASE3 0x00000206
#define MSR_IA32_MTRR_PHYSMASK3 0x00000207
#define MSR_IA32_MTRR_PHYSBASE4 0x00000208
#define MSR_IA32_MTRR_PHYSMASK4 0x00000209
#define MSR_IA32_MTRR_PHYSBASE5 0x0000020A
#define MSR_IA32_MTRR_PHYSMASK5 0x0000020B
#define MSR_IA32_MTRR_PHYSBASE6 0x0000020C
#define MSR_IA32_MTRR_PHYSMASK6 0x0000020D
#define MSR_IA32_MTRR_PHYSBASE7 0x0000020E
#define MSR_IA32_MTRR_PHYSMASK7 0x0000020F
#define MSR_IA32_MTRR_PHYSBASE8 0x00000210
#define MSR_IA32_MTRR_PHYSMASK8 0x00000211
#define MSR_IA32_MTRR_PHYSBASE9 0x00000212
#define MSR_IA32_MTRR_PHYSMASK9 0x00000213

#define MSR_IA32_EFER           0xC0000080
#define MSR_IA32_STAR           0xC0000081
#define MSR_IA32_LSTAR          0xC0000082
#define MSR_IA32_CSTAR          0xC0000083
#define MSR_IA32_FMASK          0xC0000084
#define MSR_IA32_FS_BASE        0xC0000100
#define MSR_IA32_GS_BASE        0xC0000101
#define MSR_IA32_KERNEL_GS_BASE 0xC0000102

STATIC
MSR_SHADOW_ENTRY*
FindShadowMsr(
  UINT32 Index
  )
{
  for (UINT32 i = 0; i < gMsrShadowCount; i++) {
    if (gMsrShadowTable[i].Index == Index && gMsrShadowTable[i].IsShadowed) {
      return &gMsrShadowTable[i];
    }
  }
  return NULL;
}

STATIC
VOID
AddShadowMsr(
  UINT32 Index,
  UINT64 Value,
  BOOLEAN IsReadOnly,
  BOOLEAN PassthroughOnWrite
  )
{
  if (gMsrShadowCount < MAX_SHADOW_MSRS) {
    gMsrShadowTable[gMsrShadowCount].Index = Index;
    gMsrShadowTable[gMsrShadowCount].Value = Value;
    gMsrShadowTable[gMsrShadowCount].IsShadowed = TRUE;
    gMsrShadowTable[gMsrShadowCount].IsReadOnly = IsReadOnly;
    gMsrShadowTable[gMsrShadowCount].PassthroughOnWrite = PassthroughOnWrite;
    gMsrShadowCount++;
  }
}

VOID
MsrShadowInit(
  VOID
  )
{
  UINT64 msrValue;
  gMsrShadowCount = 0;

  AddShadowMsr(MSR_IA32_TSC, 0, FALSE, FALSE);
  AddShadowMsr(MSR_IA32_TSC_ADJUST, 0, FALSE, TRUE);
  AddShadowMsr(MSR_IA32_APIC_BASE, AsmReadMsr64(MSR_IA32_APIC_BASE), FALSE, TRUE);

  AddShadowMsr(MSR_IA32_SYSENTER_CS, AsmReadMsr64(MSR_IA32_SYSENTER_CS), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_SYSENTER_ESP, AsmReadMsr64(MSR_IA32_SYSENTER_ESP), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_SYSENTER_EIP, AsmReadMsr64(MSR_IA32_SYSENTER_EIP), FALSE, TRUE);

  AddShadowMsr(MSR_IA32_STAR, AsmReadMsr64(MSR_IA32_STAR), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_LSTAR, AsmReadMsr64(MSR_IA32_LSTAR), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_CSTAR, AsmReadMsr64(MSR_IA32_CSTAR), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_FMASK, AsmReadMsr64(MSR_IA32_FMASK), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_FS_BASE, AsmReadMsr64(MSR_IA32_FS_BASE), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_GS_BASE, AsmReadMsr64(MSR_IA32_GS_BASE), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_KERNEL_GS_BASE, AsmReadMsr64(MSR_IA32_KERNEL_GS_BASE), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_EFER, AsmReadMsr64(MSR_IA32_EFER), FALSE, TRUE);

  AddShadowMsr(MSR_IA32_MCG_CAP, AsmReadMsr64(MSR_IA32_MCG_CAP), TRUE, FALSE);
  AddShadowMsr(MSR_IA32_MCG_STATUS, 0, FALSE, TRUE);
  AddShadowMsr(MSR_IA32_MCG_CTL, AsmReadMsr64(MSR_IA32_MCG_CTL), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_PAT, AsmReadMsr64(MSR_IA32_PAT), FALSE, TRUE);
  AddShadowMsr(MSR_IA32_MTRR_DEF_TYPE, AsmReadMsr64(MSR_IA32_MTRR_DEF_TYPE), FALSE, TRUE);

  msrValue = AsmReadMsr64(MSR_IA32_MTRR_CAP);
  UINT32 mtrrCount = (UINT32)(msrValue & 0xFF);
  AddShadowMsr(MSR_IA32_MTRR_CAP, msrValue, TRUE, FALSE);

  for (UINT32 i = 0; i < mtrrCount && i < 10; i++) {
    UINT32 baseMsr = MSR_IA32_MTRR_PHYSBASE0 + (i * 2);
    UINT32 maskMsr = MSR_IA32_MTRR_PHYSMASK0 + (i * 2);
    AddShadowMsr(baseMsr, AsmReadMsr64(baseMsr), FALSE, TRUE);
    AddShadowMsr(maskMsr, AsmReadMsr64(maskMsr), FALSE, TRUE);
  }

  DEBUG((DEBUG_INFO, "MSR: Shadow table initialized with %d entries (%d MTRRs)\n",
    gMsrShadowCount, mtrrCount));
}

EFI_STATUS
MsrShadowRead(
  IN  UINT32  Index,
  OUT UINT64  *Value
  )
{
  MSR_SHADOW_ENTRY *entry = FindShadowMsr(Index);
  if (entry != NULL) {
    *Value = entry->Value;
    return EFI_SUCCESS;
  }

  *Value = AsmReadMsr64(Index);
  return EFI_SUCCESS;
}

EFI_STATUS
MsrShadowWrite(
  IN UINT32 Index,
  IN UINT64 Value
  )
{
  MSR_SHADOW_ENTRY *entry = FindShadowMsr(Index);
  if (entry != NULL) {
    if (entry->IsReadOnly) {
      return EFI_ACCESS_DENIED;
    }

    entry->Value = Value;

    if (entry->PassthroughOnWrite) {
      AsmWriteMsr64(Index, Value);
    }

    return EFI_SUCCESS;
  }

  AsmWriteMsr64(Index, Value);
  return EFI_SUCCESS;
}

BOOLEAN
MsrIsPassThrough(
  UINT32 Index
  )
{
  return FindShadowMsr(Index) == NULL;
}
