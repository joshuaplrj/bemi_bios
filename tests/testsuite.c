#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

typedef EFI_STATUS (*TEST_FUNC)(VOID);

typedef struct {
  CHAR8       Name[64];
  TEST_FUNC   Func;
  BOOLEAN     Passed;
} TEST_ENTRY;

STATIC UINT32 gTestCount = 0;
STATIC UINT32 gTestPassed = 0;
STATIC UINT32 gTestFailed = 0;

STATIC
EFI_STATUS
TestCpuidSpoof(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x00, &eax, &ebx, &ecx, &edx);
  CHAR8 vendor[13];
  *(UINT32 *)&vendor[0] = ebx;
  *(UINT32 *)&vendor[4] = edx;
  *(UINT32 *)&vendor[8] = ecx;
  vendor[12] = '\0';

  if (AsciiStrCmp(vendor, "GenuineIntel") == 0 || AsciiStrCmp(vendor, "AuthenticAMD") == 0) {
    return EFI_SUCCESS;
  }
  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
TestVmxDetection(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x01, &eax, &ebx, &ecx, &edx);
  if (ecx & BIT5) {
    return EFI_SUCCESS;
  }
  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
TestSvmDetection(
  VOID
  )
{
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x80000001, &eax, &ebx, &ecx, &edx);
  if (ecx & BIT2) {
    return EFI_SUCCESS;
  }
  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
TestMemoryMap(
  VOID
  )
{
  UINT64 *testBuffer = AllocateZeroPool(sizeof(UINT64));
  if (testBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  FreePool(testBuffer);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
TestMsrShadow(
  VOID
  )
{
  (VOID)AsmReadMsr64(0xC0000080);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
TestApicShadow(
  VOID
  )
{
  UINT64 apicBase;
  apicBase = AsmReadMsr64(0x1B);
  if ((apicBase & 0xFFFFF000) == 0xFEE00000) {
    return EFI_SUCCESS;
  }
  return EFI_DEVICE_ERROR;
}

STATIC TEST_ENTRY gTests[] = {
  { "CPUID Vendor String",  TestCpuidSpoof,    FALSE },
  { "VMX Detection",        TestVmxDetection,   FALSE },
  { "SVM Detection",        TestSvmDetection,   FALSE },
  { "Memory Map Test",      TestMemoryMap,      FALSE },
  { "MSR Shadow Read",      TestMsrShadow,      FALSE },
  { "APIC Base Detection",  TestApicShadow,     FALSE },
};

VOID
TestRunnerInit(
  VOID
  )
{
  gTestCount = sizeof(gTests) / sizeof(gTests[0]);
  gTestPassed = 0;
  gTestFailed = 0;
  DEBUG((DEBUG_INFO, "TEST: Suite initialized with %d tests\n", gTestCount));
}

EFI_STATUS
TestRunnerExecuteAll(
  VOID
  )
{
  EFI_STATUS status;
  UINT32 testCount = sizeof(gTests) / sizeof(gTests[0]);

  DEBUG((DEBUG_INFO, "\n=== BEMI Test Suite ===\n"));

  for (UINT32 i = 0; i < testCount; i++) {
    status = gTests[i].Func();
    gTests[i].Passed = !EFI_ERROR(status);

    if (gTests[i].Passed) {
      gTestPassed++;
      DEBUG((DEBUG_INFO, "  [PASS] %s\n", gTests[i].Name));
    } else {
      gTestFailed++;
      DEBUG((DEBUG_INFO, "  [FAIL] %s (status: %r)\n", gTests[i].Name, status));
    }
  }

  DEBUG((DEBUG_INFO, "\nResults: %d/%d passed, %d failed\n",
    gTestPassed, gTestCount, gTestFailed));

  return (gTestFailed == 0) ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}
