#ifndef BEMI_API_H
#define BEMI_API_H

#include <Uefi.h>
#include <Protocol/BemiProtocol.h>

#define BEMI_MAJOR_VERSION 1
#define BEMI_MINOR_VERSION 3

typedef enum {
  BemiBootModeNative,
  BemiBootModeLegacy,
  BemiBootModeMax
} BEMI_BOOT_MODE;

typedef struct {
  UINT64  Rip;
  UINT64  Context;
} BEMI_TRACE_KEY;

typedef struct {
  UINT32  OpcodeCount;
  UINT32  MacrOpCount;
  UINT8   Data[256];
} BEMI_TRANSLATED_BLOCK;

typedef struct {
  UINT64  PhysicalAddress;
  UINT64  Size;
  UINT8   Type; 
} BEMI_MEMORY_REGION;

typedef EFI_STATUS (EFIAPI *BEMI_INITIALIZE)(
  IN  UINT64  CpuCount,
  IN  UINT64  TraceCacheSize
);

typedef EFI_STATUS (EFIAPI *BEMI_LAUNCH_VM)(
  IN  UINT64  EntryPoint,
  IN  UINT64  StackPointer
);

typedef EFI_STATUS (EFIAPI *BEMI_INTERCEPT_CPUID)(
  IN  UINT32  Leaf,
  IN  UINT32  Subleaf,
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
);

typedef EFI_STATUS (EFIAPI *BEMI_MSR_EMULATE)(
  IN  UINT32  MsrIndex,
  IN  BOOLEAN IsWrite,
  IN  OUT UINT64 *Value
);

#endif
