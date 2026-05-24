#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>
#include <IndustryStandard/Acpi.h>

#pragma pack(1)

typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER Header;
  UINT32                      LocalApicAddress;
  UINT32                      Flags;
} MADT_TABLE_HEADER;

#pragma pack()

STATIC
UINT8
AcpiCalcChecksum (
  IN VOID   *Buffer,
  IN UINTN  Size
  )
{
  UINT8 *ptr = (UINT8 *)Buffer;
  UINT8 sum = 0;
  for (UINTN i = 0; i < Size; i++) {
    sum += ptr[i];
  }
  return (UINT8)(256 - sum);
}

EFI_STATUS
AcpiInitTables (
  IN UINT32 CpuCount
  )
{
  EFI_STATUS              status;
  EFI_ACPI_TABLE_PROTOCOL *AcpiTableProtocol;
  UINTN                   MadtSize;
  MADT_TABLE_HEADER       *Madt;
  UINT8                   *CurrentPtr;
  UINTN                   TableKey;

  DEBUG((DEBUG_INFO, "ACPI: Initialising tables for %d CPUs\n", CpuCount));

  // Calculate size for MADT header + Local APIC structures + 1 IO APIC structure
  MadtSize = sizeof(MADT_TABLE_HEADER) + 
             (CpuCount * sizeof(EFI_ACPI_6_0_PROCESSOR_LOCAL_APIC_STRUCTURE)) +
             sizeof(EFI_ACPI_6_0_IO_APIC_STRUCTURE);

  Madt = AllocateZeroPool(MadtSize);
  if (Madt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Populate MADT Header
  Madt->Header.Signature = EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE;
  Madt->Header.Length = (UINT32)MadtSize;
  Madt->Header.Revision = 3;
  CopyMem(Madt->Header.OemId, "BEMI  ", 6);
  Madt->Header.OemTableId = SIGNATURE_64('B','E','M','I','B','I','O','S');
  Madt->Header.OemRevision = 1;
  Madt->Header.CreatorId = SIGNATURE_32('B','E','M','I');
  Madt->Header.CreatorRevision = 1;

  Madt->LocalApicAddress = 0xFEE00000;
  Madt->Flags = 1; // PCAT_COMPAT

  CurrentPtr = (UINT8 *)(Madt + 1);

  // Populate Processor Local APIC structures
  for (UINT32 i = 0; i < CpuCount; i++) {
    EFI_ACPI_6_0_PROCESSOR_LOCAL_APIC_STRUCTURE *Lepic = (EFI_ACPI_6_0_PROCESSOR_LOCAL_APIC_STRUCTURE *)CurrentPtr;
    Lepic->Type = EFI_ACPI_6_0_PROCESSOR_LOCAL_APIC;
    Lepic->Length = sizeof(EFI_ACPI_6_0_PROCESSOR_LOCAL_APIC_STRUCTURE);
    Lepic->AcpiProcessorId = (UINT8)i;
    Lepic->ApicId = (UINT8)i;
    Lepic->Flags = 1; // Enabled
    CurrentPtr += sizeof(EFI_ACPI_6_0_PROCESSOR_LOCAL_APIC_STRUCTURE);
  }

  // Populate IO APIC structure
  EFI_ACPI_6_0_IO_APIC_STRUCTURE *IoApic = (EFI_ACPI_6_0_IO_APIC_STRUCTURE *)CurrentPtr;
  IoApic->Type = EFI_ACPI_6_0_IO_APIC;
  IoApic->Length = sizeof(EFI_ACPI_6_0_IO_APIC_STRUCTURE);
  IoApic->IoApicId = (UINT8)CpuCount; // Next available ID
  IoApic->IoApicAddress = 0xFEC00000;
  IoApic->GlobalSystemInterruptBase = 0;

  // Calculate checksum
  Madt->Header.Checksum = 0;
  Madt->Header.Checksum = AcpiCalcChecksum(Madt, MadtSize);

  // Locate ACPI Table Protocol
  status = gBS->LocateProtocol(
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (!EFI_ERROR(status)) {
    // Install MADT using protocol
    status = AcpiTableProtocol->InstallAcpiTable(
                                  AcpiTableProtocol,
                                  Madt,
                                  MadtSize,
                                  &TableKey
                                  );
    if (EFI_ERROR(status)) {
      DEBUG((DEBUG_ERROR, "ACPI: Failed to install MADT table: %r\n", status));
    } else {
      DEBUG((DEBUG_INFO, "ACPI: Installed MADT successfully via protocol\n"));
    }
  } else {
    // If protocol not found, expose via System Configuration Table
    DEBUG((DEBUG_WARN, "ACPI: Table Protocol not found, installing config table fallback\n"));
    status = gBS->InstallConfigurationTable(&gEfiAcpiTableGuid, Madt);
  }

  FreePool(Madt);
  return status;
}
