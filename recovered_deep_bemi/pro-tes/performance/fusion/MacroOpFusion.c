#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define MAX_FUSION_PAIRS 32
#define FUSION_BUFFER_SIZE 512

typedef enum {
  FusionTypeCmpJcc,
  FusionTypeTestJcc,
  FusionTypeDecJcc,
  FusionTypeAddLea,
  FusionTypeMovCmp,
  FusionTypeNone
} FUSION_TYPE;

typedef struct {
  UINT64      Rip;
  UINT8       Opcode1;
  UINT8       Opcode2;
  UINT8       Opcode3;
  UINT8       Length;
  FUSION_TYPE Type;
  BOOLEAN     Fused;
} FUSION_PAIR;

typedef struct {
  UINT64      Rip;
  UINT64      TargetRip;
  UINT8       Condition;
  UINT8       ComparisonType;
  UINT64      ComparisonValue;
  UINT32      ExecutionCount;
  BOOLEAN     Fused;
} FUSED_MACRO_OP;

STATIC FUSION_PAIR   gFusionPairs[MAX_FUSION_PAIRS];
STATIC UINT32        gFusionPairCount = 0;
STATIC FUSED_MACRO_OP gFusionBuffer[FUSION_BUFFER_SIZE];
STATIC UINT32        gFusionBufferCount = 0;
STATIC UINT64        gFusionAttempts = 0;
STATIC UINT64        gFusionSuccesses = 0;

VOID
MacroOpFusionInit(
  VOID
  )
{
  ZeroMem(gFusionPairs, sizeof(gFusionPairs));
  ZeroMem(gFusionBuffer, sizeof(gFusionBuffer));
  gFusionPairCount = 0;
  gFusionBufferCount = 0;
  gFusionAttempts = 0;
  gFusionSuccesses = 0;
  DEBUG((DEBUG_INFO, "FUSION: Macro-op fusion pipeline initialized\n"));
}

BOOLEAN
MacroOpFusionIsFusable(
  IN UIN
<truncated 1325 bytes>

MacroOpFusionTryFuse(
  IN UINT64  Rip1,
  IN UINT8   Opcode1,
  IN UINT8   Length1,
  IN UINT64  Rip2,
  IN UINT8   Opcode2,
  IN UINT8   Opcode2Extra,
  IN UINT8   Length2
  )
{
  gFusionAttempts++;

  if (!MacroOpFusionIsFusable(Opcode1, Opcode2, Opcode2Extra)) {
    return EFI_UNSUPPORTED;
  }

  if (gFusionPairCount < MAX_FUSION_PAIRS) {
    FUSION_PAIR *pair = &gFusionPairs[gFusionPairCount];
    pair->Rip = Rip1;
    pair->Opcode1 = Opcode1;
    pair->Opcode2 = Opcode2;
    pair->Opcode3 = Opcode2Extra;
    pair->Length = Length1 + Length2;
    pair->Type = MacroOpFusionGetType(Opcode1, Opcode2, Opcode2Extra);
    pair->Fused = TRUE;
    gFusionPairCount++;
  }

  if (gFusionBufferCount < FUSION_BUFFER_SIZE) {
    FUSED_MACRO_OP *mop = &gFusionBuffer[gFusionBufferCount];
    mop->Rip = Rip1;
    mop->TargetRip = Rip2 + Length2;
    mop->ComparisonType = Opcode1;
    mop->Fused = TRUE;
    gFusionBufferCount++;
  }

  gFusionSuccesses++;
  return EFI_SUCCESS;
}

BOOLEAN
MacroOpFusionLookup(
  IN  UINT64 Rip,
  OUT FUSED_MACRO_OP *Result
  )
{
  for (UINT32 i = 0; i < gFusionBufferCount; i++) {
    if (gFusionBuffer[i].Rip == Rip && gFusionBuffer[i].Fused) {
      CopyMem(Result, &gFusionBuffer[i], sizeof(FUSED_MACRO_OP));
      gFusionBuffer[i].ExecutionCount++;
      return TRUE;
    }
  }
  return FALSE;
}

VOID
MacroOpFusionGetStats(
  OUT UINT64 *Attempts,
  OUT UINT64 *Successes,
  OUT double  *FusionRate
  )
{
  *Attempts = gFusionAttempts;
  *Successes = gFusionSuccesses;
  *FusionRate = (gFusionAttempts > 0) ?
    (double)gFusionSuccesses / (double)gFusionAttempts : 0.0;
}
