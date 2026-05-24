#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Smbios.h>
#include <IndustryStandard/SmBios.h>

EFI_STATUS
SmbiosInitTables (
  IN UINT32 CpuCount
  )
{
  EFI_STATUS          status;
  EFI_SMBIOS_PROTOCOL *Smbios;
  EFI_SMBIOS_HANDLE   SmbiosHandle;

  DEBUG((DEBUG_INFO, "SMBIOS: Initialising tables for %d CPUs\n", CpuCount));

  // Locate the SMBIOS protocol
  status = gBS->LocateProtocol(
                  &gEfiSmbiosProtocolGuid,
                  NULL,
                  (VOID **)&Smbios
                  );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_WARN, "SMBIOS: Protocol not found: %r\n", status));
    return status;
  }

  // Create Type 0 (BIOS Information) Structure
  struct {
    SMBIOS_TABLE_TYPE0  Base;
    CHAR8               Strings[64];
  } T0;
  
  ZeroMem(&T0, sizeof(T0));
  T0.Base.Hdr.Type = SMBIOS_TYPE_BIOS_INFORMATION;
  T0.Base.Hdr.Length = sizeof(SMBIOS_TABLE_TYPE0);
  T0.Base.Vendor = 1;      // First string
  T0.Base.BiosVersion = 2; // Second string
  
  // Format string block (each string is null-terminated, block is double-null terminated)
  AsciiStrCpyS(T0.Strings, sizeof(T0.Strings), "BEMI");
  UINTN s1_len = AsciiStrSize("BEMI");
  AsciiStrCpyS(T0.Strings + s1_len, sizeof(T0.Strings) - s1_len, "v7.2");
  UINTN s2_len = AsciiStrSize("v7.2");
  T0.Strings[s1_len + s2_len] = '\0'; // Double null termination

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  status = Smbios->Add(
                     Smbios,
                     NULL,
                     &SmbiosHandle,
                     (EFI_SMBIOS_TABLE_HEADER *)&T0
                     );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "SMBIOS: Failed to add Type 0 structure: %r\n", status));
  } else {
    DEBUG((DEBUG_INFO, "SMBIOS: Added Type 0 structure\n"));
  }

  // Create Type 1 (System Information) Structure
  struct {
    SMBIOS_TABLE_TYPE1  Base;
    CHAR8               Strings[64];
  } T1;

  ZeroMem(&T1, sizeof(T1));
  T1.Base.Hdr.Type = SMBIOS_TYPE_SYSTEM_INFORMATION;
  T1.Base.Hdr.Length = sizeof(SMBIOS_TABLE_TYPE1);
  T1.Base.Manufacturer = 1;
  T1.Base.ProductName = 2;
  
  AsciiStrCpyS(T1.Strings, sizeof(T1.Strings), "BEMI Hardware");
  s1_len = AsciiStrSize("BEMI Hardware");
  AsciiStrCpyS(T1.Strings + s1_len, sizeof(T1.Strings) - s1_len, "BEMI Box");
  s2_len = AsciiStrSize("BEMI Box");
  T1.Strings[s1_len + s2_len] = '\0';

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  status = Smbios->Add(
                     Smbios,
                     NULL,
                     &SmbiosHandle,
                     (EFI_SMBIOS_TABLE_HEADER *)&T1
                     );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "SMBIOS: Failed to add Type 1 structure: %r\n", status));
  } else {
    DEBUG((DEBUG_INFO, "SMBIOS: Added Type 1 structure\n"));
  }

  // Create Type 4 (Processor Information) Structure
  struct {
    SMBIOS_TABLE_TYPE4  Base;
    CHAR8               Strings[64];
  } T4;

  ZeroMem(&T4, sizeof(T4));
  T4.Base.Hdr.Type = SMBIOS_TYPE_PROCESSOR_INFORMATION;
  T4.Base.Hdr.Length = sizeof(SMBIOS_TABLE_TYPE4);
  T4.Base.Socket = 1;
  T4.Base.ProcessorManufacturer = 2;
  T4.Base.ProcessorVersion = 3;
  T4.Base.Status = 1; // Enabled
  T4.Base.ProcessorUpgrade = ProcessorUpgradeNone;
  T4.Base.CoreCount = (UINT8)CpuCount;
  T4.Base.EnabledCoreCount = (UINT8)CpuCount;
  T4.Base.ThreadCount = (UINT8)CpuCount;

  AsciiStrCpyS(T4.Strings, sizeof(T4.Strings), "Socket BEMI");
  s1_len = AsciiStrSize("Socket BEMI");
  AsciiStrCpyS(T4.Strings + s1_len, sizeof(T4.Strings) - s1_len, "BEMI Corp");
  s2_len = AsciiStrSize("BEMI Corp");
  AsciiStrCpyS(T4.Strings + s1_len + s2_len, sizeof(T4.Strings) - s1_len - s2_len, "BEMI Processor");
  UINTN s3_len = AsciiStrSize("BEMI Processor");
  T4.Strings[s1_len + s2_len + s3_len] = '\0';

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  status = Smbios->Add(
                     Smbios,
                     NULL,
                     &SmbiosHandle,
                     (EFI_SMBIOS_TABLE_HEADER *)&T4
                     );
  if (EFI_ERROR(status)) {
    DEBUG((DEBUG_ERROR, "SMBIOS: Failed to add Type 4 structure: %r\n", status));
  } else {
    DEBUG((DEBUG_INFO, "SMBIOS: Added Type 4 structure\n"));
  }

  return EFI_SUCCESS;
}
