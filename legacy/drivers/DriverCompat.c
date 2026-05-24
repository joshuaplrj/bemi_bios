#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

EFI_STATUS
DriverCompatRegisterRom (
  IN UINT64 RomAddress,
  IN UINT32 RomSize
  )
{
  DEBUG((DEBUG_INFO, "DRIVER_COMPAT: Registered Legacy Option ROM at 0x%llx (size=%d bytes)\n",
    RomAddress, RomSize));
  return EFI_SUCCESS;
}
