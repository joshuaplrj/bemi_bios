#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

/**
  Hardware Memory Link Compression (HMC) — Bemi v3.0
  ======================================================
  UEFI model of Base-Delta-Immediate (BDI) hardware memory compression.
  
  BDI operates in the memory controller's physical link layer:
    - Automatically compresses cache lines on write
    - Low-latency decompression pipeline (1-cycle read penalty)
    - Target average compression ratio of 1.5x, extending dual-channel DDR5 
      physical bandwidth (64 GB/s) to 96.0 GB/s effective bandwidth.
**/

#define HMC_CACHE_LINE_SIZE  64
#define HMC_MIN_RATIO        1.0
#define HMC_MAX_RATIO        4.0

typedef enum {
  HmcPatternGeneric = 0,
  HmcPatternVideoEncoding = 1,
  HmcPatternOlapScan = 2,
  HmcPatternDlTraining = 3
} HMC_WORKLOAD_PATTERN;

typedef struct {
  double    TargetCompressionRatio;
  double    PeakPhysicalBwGbs;
  double    PeakEffectiveBwGbs;
  
  UINT64    TotalRawBytesRead;
  UINT64    TotalRawBytesWritten;
  UINT64    TotalCompressedBytesRead;
  UINT64    TotalCompressedBytesWritten;
  
  UINT64    CacheLinesCompressed;
  UINT64    CacheLinesDecompressed;
  UINT64    DecompressionLatencyCycles;
  
  BOOLEAN   Initialized;
} MEMORY_COMPRESSOR;

STATIC MEMORY_COMPRES
<truncated 5759 bytes>
:\n"));
  DEBUG((DEBUG_INFO, "    Read (Decompress): %.2fx\n", readRatio));
  DEBUG((DEBUG_INFO, "    Write (Compress) : %.2fx\n", writeRatio));
  DEBUG((DEBUG_INFO, "    Overall Blended  : %.2fx\n", overallRatio));
  DEBUG((DEBUG_INFO, "  Effective Bandwidth: %.2f GB/s (vs physical %.1f GB/s)\n",
    gMemComp->PeakPhysicalBwGbs * overallRatio, gMemComp->PeakPhysicalBwGbs));
}

/**
  Configure Adaptive Hardware Memory Compression (Adaptive HMC) for Bemi v4.0.
  
  Selects FPC/FDC compression pattern based on workload type, modifying target ratio.
**/
EFI_STATUS
MemoryCompressorSetWorkloadPattern(
  IN HMC_WORKLOAD_PATTERN Pattern
  )
{
  if (gMemComp == NULL || !gMemComp->Initialized) {
    return EFI_NOT_READY;
  }

  double NewRatio = 1.5;
  CHAR8 *PatternName = "Generic FPC/FDC";

  switch (Pattern) {
    case HmcPatternVideoEncoding:
      NewRatio = 1.8;
      PatternName = "Video Pixel Stream (1.8x)";
      break;
    case HmcPatternOlapScan:
      NewRatio = 2.0;
      PatternName = "OLAP Columns (2.0x)";
      break;
    case HmcPatternDlTraining:
      NewRatio = 2.2;
      PatternName = "DL Training Tensors (2.2x)";
      break;
    case HmcPatternGeneric:
    default:
      NewRatio = 1.5;
      PatternName = "Generic FPC/FDC (1.5x)";
      break;
  }

  gMemComp->TargetCompressionRatio = NewRatio;
  gMemComp->PeakEffectiveBwGbs = gMemComp->PeakPhysicalBwGbs * NewRatio;

  DEBUG((DEBUG_INFO, "HMC [v4.0]: Adaptive compression switched to pattern: %s\n", PatternName));
  DEBUG((DEBUG_INFO, "HMC [v4.0]: Effective Peak Bandwidth is now %.1f GB/s\n", gMemComp->PeakEffectiveBwGbs));

  return EFI_SUCCESS;
}

