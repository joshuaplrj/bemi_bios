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

The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.