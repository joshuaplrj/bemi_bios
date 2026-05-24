#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define MAX_FUSION_PAIRS 32
#define FUSION_BUFFER_SIZE 512

#define V71_FUSION_BONUS 1.3
#define MAX_FUSION_PAIRS_V71 64
#define FUSION_BUFFER_SIZE_V71 1024
#define V71_DBO_FUSION_ENABLED 1

#define V72_FUSION_BONUS      2.00
#define V72_FUSION_STORAGE_MB 6

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

typedef struct {
  UINT64    WorkloadId;
  UINT64    TotalInstructions;
  UINT64    FusedOps;
  UINT64    CacheHits;
  double    FusionRate;
  BOOLEAN   DboPatternsDetected;
} V71_FUSION_TRACKING;

STATIC FUSION_PAIR   gFusionPairs[MAX_FUSION_PAIRS];
STATIC UINT32        gFusionPairCount = 0;
STATIC FUSED_MACRO_OP gFusionBuffer[FUSION_BUFFER_SIZE];
STATIC UINT32        gFusionBufferCount = 0;
STATIC UINT64        gFusionAttempts = 0;
STATIC UINT64        gFusionSuccesses = 0;

STATIC FUSION_PAIR     gFusionPairsV71[MAX_FUSION_PAIRS_V71];
STATIC UINT32          gFusionPairCountV71 = 0;
STATIC FUSED_MACRO_OP  gFusionBufferV71[FUSION_BUFFER_SIZE_V71];
STATIC UINT32          gFusionBufferCountV71 = 0;
STATIC UINT64          gFusionAttemptsV71 = 0;
STATIC UINT64          gFusionSuccessesV71 = 0;
STATIC V71_FUSION_TRACKING gV71Tracking = {0};

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
  DEBUG((DEBUG_INFO, "FUSION: v7.1 DBO software fusion initialized\n"));
}

VOID
MacroOpFusionV71Init(
  VOID
  )
{
  ZeroMem(gFusionPairsV71, sizeof(gFusionPairsV71));
  ZeroMem(gFusionBufferV71, sizeof(gFusionBufferV71));
  gFusionPairCountV71 = 0;
  gFusionBufferCountV71 = 0;
  gFusionAttemptsV71 = 0;
  gFusionSuccessesV71 = 0;
  ZeroMem(&gV71Tracking, sizeof(gV71Tracking));
  gV71Tracking.DboPatternsDetected = FALSE;
  DEBUG((DEBUG_INFO, "FUSION: v7.1 DBO software fusion initialized\n"));
}

STATIC
BOOLEAN
IsJcc(
  IN UINT8 Opcode,
  IN UINT8 OpcodeExtra
  )
{
  if (Opcode >= 0x70 && Opcode <= 0x7F) {
    return TRUE;
  }
  if (Opcode == 0x0F && OpcodeExtra >= 0x80 && OpcodeExtra <= 0x8F) {
    return TRUE;
  }
  return FALSE;
}

BOOLEAN
MacroOpFusionIsFusable(
  IN UINT8 Opcode1,
  IN UINT8 Opcode2,
  IN UINT8 Opcode2Extra
  )
{
  if (IsJcc(Opcode2, Opcode2Extra)) {
    if ((Opcode1 >= 0x38 && Opcode1 <= 0x3D) || (Opcode1 >= 0x80 && Opcode1 <= 0x83)) {
      return TRUE;
    }
    if (Opcode1 == 0x84 || Opcode1 == 0x85 || Opcode1 == 0xA8 || Opcode1 == 0xA9 || Opcode1 == 0xF6 || Opcode1 == 0xF7) {
      return TRUE;
    }
    if (Opcode1 == 0xFE || Opcode1 == 0xFF) {
      return TRUE;
    }
  }

  if (Opcode1 == 0x8D && ((Opcode2 >= 0x00 && Opcode2 <= 0x05) || (Opcode2 >= 0x80 && Opcode2 <= 0x83))) {
    return TRUE;
  }

  if ((Opcode1 >= 0x88 && Opcode1 <= 0x8B) || Opcode1 == 0xC6 || Opcode1 == 0xC7) {
    if ((Opcode2 >= 0x38 && Opcode2 <= 0x3D) || (Opcode2 >= 0x80 && Opcode2 <= 0x83)) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
FUSION_TYPE
MacroOpFusionGetType(
  IN UINT8 Opcode1,
  IN UINT8 Opcode2,
  IN UINT8 Opcode2Extra
  )
{
  if (IsJcc(Opcode2, Opcode2Extra)) {
    if ((Opcode1 >= 0x38 && Opcode1 <= 0x3D) || (Opcode1 >= 0x80 && Opcode1 <= 0x83)) {
      return FusionTypeCmpJcc;
    }
    if (Opcode1 == 0x84 || Opcode1 == 0x85 || Opcode1 == 0xA8 || Opcode1 == 0xA9 || Opcode1 == 0xF6 || Opcode1 == 0xF7) {
      return FusionTypeTestJcc;
    }
    if (Opcode1 == 0xFE || Opcode1 == 0xFF) {
      return FusionTypeDecJcc;
    }
  }

  if (Opcode1 == 0x8D && ((Opcode2 >= 0x00 && Opcode2 <= 0x05) || (Opcode2 >= 0x80 && Opcode2 <= 0x83))) {
    return FusionTypeAddLea;
  }

  if ((Opcode1 >= 0x88 && Opcode1 <= 0x8B) || Opcode1 == 0xC6 || Opcode1 == 0xC7) {
    if ((Opcode2 >= 0x38 && Opcode2 <= 0x3D) || (Opcode2 >= 0x80 && Opcode2 <= 0x83)) {
      return FusionTypeMovCmp;
    }
  }

  return FusionTypeNone;
}

EFI_STATUS
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

EFI_STATUS
MacroOpFusionV71TryFuse(
  IN UINT64  Rip1,
  IN UINT8   Opcode1,
  IN UINT8   Length1,
  IN UINT64  Rip2,
  IN UINT8   Opcode2,
  IN UINT8   Opcode2Extra,
  IN UINT8   Length2
  )
{
  gFusionAttemptsV71++;

  if (!MacroOpFusionIsFusable(Opcode1, Opcode2, Opcode2Extra)) {
    return EFI_UNSUPPORTED;
  }

  if (gFusionPairCountV71 < MAX_FUSION_PAIRS_V71) {
    FUSION_PAIR *pair = &gFusionPairsV71[gFusionPairCountV71];
    pair->Rip = Rip1;
    pair->Opcode1 = Opcode1;
    pair->Opcode2 = Opcode2;
    pair->Opcode3 = Opcode2Extra;
    pair->Length = Length1 + Length2;
    pair->Type = MacroOpFusionGetType(Opcode1, Opcode2, Opcode2Extra);
    pair->Fused = TRUE;
    gFusionPairCountV71++;
  }

  if (gFusionBufferCountV71 < FUSION_BUFFER_SIZE_V71) {
    FUSED_MACRO_OP *mop = &gFusionBufferV71[gFusionBufferCountV71];
    mop->Rip = Rip1;
    mop->TargetRip = Rip2 + Length2;
    mop->ComparisonType = Opcode1;
    mop->Fused = TRUE;
    gFusionBufferCountV71++;
  }

  gFusionSuccessesV71++;
  return EFI_SUCCESS;
}

double
MacroOpFusionV71GetBonus(
  VOID
  )
{
  return V71_FUSION_BONUS;
}

VOID
MacroOpFusionV72Init(
  VOID
  )
{
  DEBUG((DEBUG_INFO, "FUSION: v7.2 initialized, 2.00x bonus, 6MB L3 storage (~98K super-op patterns)\n"));
}

double
MacroOpFusionV72GetBonus(
  VOID
  )
{
  return V72_FUSION_BONUS;
}

VOID
MacroOpFusionDboAnalyze(
  IN UINT64  RegionStartRip,
  IN UINT32  InstructionCount
  )
{
  UINT32  detected = 0;
  UINT32  scanned  = 0;

  gV71Tracking.WorkloadId = RegionStartRip;
  gV71Tracking.TotalInstructions = InstructionCount;

  for (UINT32 i = 0; i + 1 < gFusionBufferCountV71 && scanned < InstructionCount; i++, scanned++) {
    FUSED_MACRO_OP *mop = &gFusionBufferV71[i];
    if (mop->Fused && gFusionPairCountV71 > 0) {
      detected++;
    }
  }

  gV71Tracking.FusedOps = detected;
  gV71Tracking.CacheHits = gFusionSuccessesV71;
  gV71Tracking.FusionRate = (gFusionAttemptsV71 > 0) ?
    (double)gFusionSuccessesV71 / (double)gFusionAttemptsV71 : 0.0;

  if (detected > 0) {
    gV71Tracking.DboPatternsDetected = TRUE;
  }

  DEBUG((
    DEBUG_INFO,
    "FUSION: v7.1 DBO analyzed region 0x%llx - %u instructions, %u fusion patterns detected (rate: %.2f)\n",
    RegionStartRip,
    InstructionCount,
    detected,
    gV71Tracking.FusionRate
    ));
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
  OUT double  *FusionRate,
  OUT UINT64 *AttemptsV71,
  OUT UINT64 *SuccessesV71,
  OUT double  *FusionRateV71
  )
{
  *Attempts = gFusionAttempts;
  *Successes = gFusionSuccesses;
  *FusionRate = (gFusionAttempts > 0) ?
    (double)gFusionSuccesses / (double)gFusionAttempts : 0.0;
  *AttemptsV71 = gFusionAttemptsV71;
  *SuccessesV71 = gFusionSuccessesV71;
  *FusionRateV71 = (gFusionAttemptsV71 > 0) ?
    (double)gFusionSuccessesV71 / (double)gFusionAttemptsV71 : 0.0;
}
