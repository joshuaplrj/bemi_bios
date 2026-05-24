[Defines]
  INF_VERSION                    = 0x00010006
  BASE_NAME                      = BemiBiosCore
  FILE_GUID                      = AABBCCDD-EEFF-0011-2233-445566778899
  MODULE_TYPE                    = DXE_DRIVER
  VERSION_STRING                 = 1.3
  ENTRY_POINT                    = BemiBiosEntryPoint

[Sources]
  DXE/BemiBiosCore.c
  POST/PostRoutines.c
  POST/PostAsm.nasm
  ../../hypervisor/common/HypervisorBackend.c
  ../../hypervisor/vmx/VmxCore.c
  ../../hypervisor/vmx/VmxExitAsm.nasm
  ../../hypervisor/svm/SvmCore.c
  ../../hypervisor/svm/SvmAsm.nasm
  ../../hypervisor/svm/SvmExitAsm.nasm
  ../../hwcompat/cpuid/CpuidSpoof.c
  ../../hwcompat/msr/MsrShadow.c
  ../../hwcompat/apic/ApicShadow.c
  ../../hwcompat/smm/SmmHandler.c
  ../../hwcompat/acpi/AcpiTables.c
  ../../hwcompat/smbios/SmbiosTables.c
  ../../legacy/csm/CsmModule.c
  ../../legacy/boot/BootProtocol.c
  ../../legacy/drivers/DriverCompat.c
  ../../performance/tcache/TraceCache.c
  ../../performance/tage/TagePredictor.c
  ../../performance/fusion/MacroOpFusion.c
  ../../performance/interrupt/InterruptLatency.c
  ../../performance/rob/RobDistributor.c
  ../../tests/TestSuite.c
  Protocol/BemiProtocol.c
  Protocol/BemiProtocol.h

[Packages]
  MdePkg/MdePkg.dec
  MdeModulePkg/MdeModulePkg.dec
  BemiBiosPkg/BemiBiosPkg.dec

[LibraryClasses]
  UefiDriverEntryPoint
  UefiLib
  UefiBootServicesTableLib
  UefiRuntimeServicesTableLib
  DebugLib
  BaseMemoryLib
  BaseLib
  IoLib
  CacheMaintenanceLib
  SynchronizationLib
  MemoryAllocationLib
  PcdLib
  DevicePathLib

[Protocols]
  gEfiBemiProtocolGuid

[Guids]
  gBemiTokenSpaceGuid

[Pcd]
  gBemiTokenSpaceGuid.PcdBemiBootMode
  gBemiTokenSpaceGuid.PcdBemiTraceCacheSize
  gBemiTokenSpaceGuid.PcdBemiEnable
  gBemiTokenSpaceGuid.PcdBemiEnableHypervisorExperimental

[Depex]
  TRUE
