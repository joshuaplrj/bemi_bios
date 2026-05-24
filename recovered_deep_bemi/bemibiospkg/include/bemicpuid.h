#ifndef BEMI_CPUID_H
#define BEMI_CPUID_H

#include <Base.h>

VOID
CpuidSpoofInit(
  VOID
  );

VOID
CpuidSpoofHandler(
  IN  UINT32 Leaf,
  IN  UINT32 Subleaf,
  OUT UINT32 *Eax,
  OUT UINT32 *Ebx,
  OUT UINT32 *Ecx,
  OUT UINT32 *Edx
  );

#endif
