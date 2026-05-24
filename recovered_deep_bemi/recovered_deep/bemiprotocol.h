#ifndef EFI_BEMI_PROTOCOL_H
#define EFI_BEMI_PROTOCOL_H

#include <Uefi.h>

#define EFI_BEMI_PROTOCOL_GUID \
  { 0x12345678, 0x9ABC, 0xDEF0, { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 } }

typedef struct _EFI_BEMI_PROTOCOL EFI_BEMI_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_BEMI_GET_BOOT_MODE)(
  IN  EFI_BEMI_PROTOCOL  *This,
  OUT UINT8              *BootMode
  );

typedef
EFI_STATUS
(EFIAPI *EFI_BEMI_SET_BOOT_MODE)(
  IN EFI_BEMI_PROTOCOL  *This,
  IN UINT8               BootMode
  );

typedef
EFI_STATUS
(EFIAPI *EFI_BEMI_INIT_HYPERVISOR)(
  IN EFI_BEMI_PROTOCOL  *This,
  IN UINT64              CpuCount
  );

typedef
EFI_STATUS
(EFIAPI *EFI_BEMI_HANDLE_CPUID)(
  IN  EFI_BEMI_PROTOCOL  *This,
  IN  UINT32              Leaf,
  IN  UINT32              Subleaf,
  OUT UINT32             *Eax,
  OUT UINT32             *Ebx,
  OUT UINT32             *Ecx,
  OUT UINT32             *Edx
  );

struct _EFI_BEMI_PROTOCOL {
  EFI_BEMI_GET_BOOT_MODE     GetBootMode;
  EFI_BEMI_SET_BOOT_MODE     SetBootMode;
  EFI_BEMI_INIT_HYPERVISOR   InitHypervisor;
  EFI_BEMI_HANDLE_CPUID      HandleCpuid;
};

extern EFI_GUID gEfiBemiProtocolGuid;

#endif
