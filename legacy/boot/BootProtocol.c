#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/LoadFile.h>

EFI_STATUS
BootProtocolScanAndLoad (
  OUT VOID **BootBuffer,
  OUT UINTN *BufferSize
  )
{
  EFI_STATUS            status;
  UINTN                 numHandles;
  EFI_HANDLE            *handles = NULL;
  EFI_BLOCK_IO_PROTOCOL *blockIo = NULL;
  VOID                  *buffer = NULL;
  UINTN                 size = SIZE_4KB;

  if (BootBuffer == NULL || BufferSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG((DEBUG_INFO, "BOOT: Scanning block devices for boot sector...\n"));

  status = gBS->LocateHandleBuffer(
                  ByProtocol,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &numHandles,
                  &handles
                  );
  if (EFI_ERROR(status) || numHandles == 0) {
    DEBUG((DEBUG_ERROR, "BOOT: No block devices found: %r\n", status));
    return EFI_NOT_FOUND;
  }

  // Look for the first bootable device
  for (UINTN i = 0; i < numHandles; i++) {
    status = gBS->HandleProtocol(
                    handles[i],
                    &gEfiBlockIoProtocolGuid,
                    (VOID **)&blockIo
                    );
    if (EFI_ERROR(status) || blockIo->Media->LogicalPartition) {
      continue; // Skip logical partitions, we want physical disk MBR/GPT
    }

    buffer = AllocateZeroPool(size);
    if (buffer == NULL) {
      FreePool(handles);
      return EFI_OUT_OF_RESOURCES;
    }

    // Read Sector 0 (MBR)
    status = blockIo->ReadBlocks(
                        blockIo,
                        blockIo->Media->MediaId,
                        0,
                        size,
                        buffer
                        );
    if (!EFI_ERROR(status)) {
      // Validate MBR signature (0xAA55)
      UINT16 signature = *(UINT16 *)((UINT8 *)buffer + 510);
      if (signature == 0xAA55) {
        DEBUG((DEBUG_INFO, "BOOT: Found boot sector on disk index %d!\n", i));
        *BootBuffer = buffer;
        *BufferSize = size;
        FreePool(handles);
        return EFI_SUCCESS;
      }
    }

    FreePool(buffer);
  }

  FreePool(handles);
  DEBUG((DEBUG_WARN, "BOOT: No active MBR boot partition detected\n"));
  return EFI_NOT_FOUND;
}
