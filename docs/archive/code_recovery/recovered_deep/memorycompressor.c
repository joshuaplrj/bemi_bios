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

STATIC MEMORY_COMPRESSOR *gMemComp = NULL;

/**
  Initialize the Hardware Memory Compressor.
  
  @param[in] TargetRatio     Target compression ratio (e.g., 1.5).
  @param[in] PeakPhysicalBw  Peak physical bandw
<truncated 5345 bytes>
->TotalCompressedBytesRead + gMemComp->TotalCompressedBytesWritten);
  
  double actualRatio = (totalComp > 0) ? (totalRaw / totalComp) : ratio;
  *EffectiveBwGbs = gMemComp->PeakPhysicalBwGbs * actualRatio;
}

/**
  Print Memory Compressor performance stats.
**/
VOID
MemoryCompressorPrintStats(
  VOID
  )
{
  if (gMemComp == NULL || !gMemComp->Initialized) return;

  double writeRatio = (gMemComp->TotalCompressedBytesWritten > 0) ?
    (double)gMemComp->TotalRawBytesWritten / gMemComp->TotalCompressedBytesWritten : 1.0;
    
  double readRatio = (gMemComp->TotalCompressedBytesRead > 0) ?
    (double)gMemComp->TotalRawBytesRead / gMemComp->TotalCompressedBytesRead : 1.0;

  double overallRatio = (gMemComp->TotalCompressedBytesRead + gMemComp->TotalCompressedBytesWritten > 0) ?
    (double)(gMemComp->TotalRawBytesRead + gMemComp->TotalRawBytesWritten) / 
    (gMemComp->TotalCompressedBytesRead + gMemComp->TotalCompressedBytesWritten) : 1.0;

  DEBUG((DEBUG_INFO, "HMC Stats:\n"));
  DEBUG((DEBUG_INFO, "  Lines Compressed  : %lld\n", gMemComp->CacheLinesCompressed));
  DEBUG((DEBUG_INFO, "  Lines Decompressed: %lld\n", gMemComp->CacheLinesDecompressed));
  DEBUG((DEBUG_INFO, "  Raw Read Volume   : %lld bytes\n", gMemComp->TotalRawBytesRead));
  DEBUG((DEBUG_INFO, "  Raw Write Volume  : %lld bytes\n", gMemComp->TotalRawBytesWritten));
  DEBUG((DEBUG_INFO, "  Compression Ratios:\n"));
  DEBUG((DEBUG_INFO, "    Read (Decompress): %.2fx\n", readRatio));
  DEBUG((DEBUG_INFO, "    Write (Compress) : %.2fx\n", writeRatio));
  DEBUG((DEBUG_INFO, "    Overall Blended  : %.2fx\n", overallRatio));
  DEBUG((DEBUG_INFO, "  Effective Bandwidth: %.2f GB/s (vs physical %.1f GB/s)\n",
    gMemComp->PeakPhysicalBwGbs * overallRatio, gMemComp->PeakPhysicalBwGbs));
}
