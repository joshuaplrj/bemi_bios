#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleTextIn.h>

// Mock interfaces for legacy BIOS interrupts

EFI_STATUS
CsmEmulateInt13 (
  IN UINT8  Drive,
  IN UINT8  Function,
  IN UINT64 Lba,
  IN UINT16 SectorCount,
  IN OUT VOID *Buffer
  )
{
  EFI_STATUS                  status;
  UINTN                       numHandles;
  EFI_HANDLE                  *handles = NULL;
  EFI_BLOCK_IO_PROTOCOL       *blockIo = NULL;

  DEBUG((DEBUG_VERBOSE, "CSM: INT 13h - Drive: 0x%x, Func: 0x%x, LBA: %lld, Sectors: %d\n",
    Drive, Function, Lba, SectorCount));

  // Locate BlockIO handles
  status = gBS->LocateHandleBuffer(
                  ByProtocol,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &numHandles,
                  &handles
                  );
  if (EFI_ERROR(status) || numHandles == 0) {
    return EFI_NO_MEDIA;
  }

  // Use the first BlockIO handle for drive simulation (e.g. floppy or HDD)
  status = gBS->HandleProtocol(
                  handles[0],
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&blockIo
                  );
  if (EFI_ERROR(status)) {
    FreePool(handles);
    return status;
  }

  UINT32 mediaId = blockIo->Media->MediaId;
  UINT32 blockSize = blockIo->Media->BlockSize;

  if (Function == 0x02) { // Read sectors
    status = blockIo->ReadBlocks(
                        blockIo,
                        mediaId,
                        Lba,
                        SectorCount * blockSize,
                        Buffer
                        );
  } else if (Function == 0x03) { // Write sectors
    status = blockIo->WriteBlocks(
                        blockIo,
                        mediaId,
                        Lba,
                        SectorCount * blockSize,
                        Buffer
                        );
  } else {
    status = EFI_UNSUPPORTED;
  }

  FreePool(handles);
  return status;
}

EFI_STATUS
CsmEmulateInt15E820 (
  OUT VOID   *E820MapBuffer,
  IN OUT UINT32 *MapCount
  )
{
  EFI_STATUS            status;
  UINTN                 mapSize = 0;
  EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
  UINTN                 mapKey;
  UINTN                 descriptorSize;
  UINT32                descriptorVersion;

  DEBUG((DEBUG_INFO, "CSM: Emulating INT 15h E820 Map extraction\n"));

  // Get EDK2 UEFI memory map
  status = gBS->GetMemoryMap(
                  &mapSize,
                  memoryMap,
                  &mapKey,
                  &descriptorSize,
                  &descriptorVersion
                  );
  if (status != EFI_BUFFER_TOO_SMALL) {
    return status;
  }

  memoryMap = AllocateZeroPool(mapSize + 2 * descriptorSize);
  if (memoryMap == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  status = gBS->GetMemoryMap(
                  &mapSize,
                  memoryMap,
                  &mapKey,
                  &descriptorSize,
                  &descriptorVersion
                  );
  if (EFI_ERROR(status)) {
    FreePool(memoryMap);
    return status;
  }

  // Populate map buffer
  UINT32 count = (UINT32)(mapSize / descriptorSize);
  if (count > *MapCount) {
    count = *MapCount;
  }
  
  ZeroMem(E820MapBuffer, count * 24); // Each E820 entry is 24 bytes (Base, Length, Type, Extended)
  
  UINT8 *outPtr = (UINT8 *)E820MapBuffer;
  UINT8 *mapPtr = (UINT8 *)memoryMap;

  for (UINT32 i = 0; i < count; i++) {
    EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)mapPtr;
    
    *(UINT64 *)(outPtr + 0) = desc->PhysicalStart;
    *(UINT64 *)(outPtr + 8) = desc->NumberOfPages * SIZE_4KB;
    
    // Translate UEFI memory type to E820 memory type
    UINT32 e820Type = 2; // Reserved by default
    switch (desc->Type) {
      case EfiConventionalMemory:
      case EfiLoaderCode:
      case EfiLoaderData:
      case EfiBootServicesCode:
      case EfiBootServicesData:
        e820Type = 1; // Usable RAM
        break;
      case EfiACPIReclaimMemory:
        e820Type = 3; // ACPI Reclaimable
        break;
      case EfiACPIMemoryNVS:
        e820Type = 4; // ACPI NVS
        break;
      default:
        e820Type = 2; // Reserved
        break;
    }
    
    *(UINT32 *)(outPtr + 16) = e820Type;
    *(UINT32 *)(outPtr + 20) = 1; // Extended attribute (valid entry)

    outPtr += 24;
    mapPtr += descriptorSize;
  }

  *MapCount = count;
  FreePool(memoryMap);
  return EFI_SUCCESS;
}

EFI_STATUS
CsmEmulateInt16Keyboard (
  OUT UINT16 *ScanCode
  )
{
  EFI_STATUS status;
  EFI_INPUT_KEY key;

  if (ScanCode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Use EFI Console In Simple Text Input Protocol
  status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
  if (EFI_ERROR(status)) {
    return status;
  }

  *ScanCode = (key.ScanCode << 8) | (key.UnicodeChar & 0xFF);
  return EFI_SUCCESS;
}

EFI_STATUS
CsmInit (
  VOID
  )
{
  DEBUG((DEBUG_INFO, "CSM: Compatibility Support Module Initialised\n"));
  return EFI_SUCCESS;
}
