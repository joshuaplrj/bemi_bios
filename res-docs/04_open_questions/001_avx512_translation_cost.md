# Open Question -- AVX-512 Translation Overhead in Weaponized Mode

**Filed:** 2024-11-05
**Priority:** Medium -- affects HPC/scientific computing segment
**Published doc reference:** Chapter 8 (Weaponized x86 Bemi) does not address this

---

## The Problem

When BEMI's Ring -1 DBT encounters an x86 AVX-512 instruction in the legacy OS's binary, what happens?

AVX-512 instructions are 6-12 bytes long with complex encoding (EVEX prefix, 5-bit register identifiers, opmask registers, embedded rounding, etc.). They map to operations on **512-bit vector registers** -- a data path width that BEMI's back-end RISC units may or may not support.

## Sub-Questions

### Q1: Does BEMI's back-end include 512-bit vector ALUs?

**Current assumption:** Yes, the Macro-Op Passthrough routes AVX-512 operations directly to the host system's 512-bit FMA units (since BEMI is fabricated on the same silicon as x86 in Weaponized mode).

**If yes:** Translation overhead is minimal -- the DBT just rewrites the EVEX prefix into the internal macro-op format (32 bits). The actual 512-bit data path is shared silicon.

**If no:** Each AVX-512 instruction must be decomposed into multiple 128-bit or 256-bit RISC vector operations. A single `VFMADD512` (which does `zmm = zmm * zmm + zmm` on 16 floats) would expand to:
- 4x 128-bit FMAs (M1 NEON-style), or
- 2x 256-bit FMAs (AVX2-style)

**Expansion ratio:** 2x-4x for the compute portion, plus 2x-4x for the register moves. Total expansion could reach **8x+** for AVX-512-heavy code.

### Q2: Can opmask registers be translated?

AVX-512 has 8 opmask registers (`k0-k7`) that control per-element predication. x86 implements these as dedicated mask registers with their own renaming and forwarding. BEMI has no equivalent.

**Translation options:**
1. Emulate opmask in software -> ~50 cycles per vector instruction, catastrophic for performance
2. Expand predicated instructions into explicit blend operations -> 2x code expansion per instruction
3. Add opmask registers to BEMI's RISC ISA -> extra silicon, only needed for legacy compatibility

**Preferred option:** (3) if silicon budget allows, (2) as fallback with documentation that opmask-heavy code runs at reduced performance.

### Q3: What about AVX-512 embedded rounding?

AVX-512 allows per-instruction rounding mode override (suppress exceptions, round toward zero, etc.). This is encoded in the EVEX prefix.

In a RISC back-end without x86's MXCSR state machine:
- Embedded rounding requires a separate rounding-control instruction before each vector op
- This adds 1 instruction per AVX-512 operation -> 1.5x-2x expansion for FP code
- Exception suppression (SAE) is expensive if the RISC back-end does not support it natively

## Preliminary Assessment

| Scenario | Overhead | Notes |
|---|---|---|
| 512-bit ALU present, no opmask | ~1.05x | Just prefix translation |
| 512-bit ALU present, with opmask | ~1.5x - 2.0x | Opmask emulation |
| No 512-bit ALU, no opmask | ~2.0x - 4.0x | 4-to-1 decomposition |
| No 512-bit ALU, with opmask | ~4.0x - 8.0x | Worst case |

## Suggested Investigation

1. Determine whether the target fabrication process (6nm) can support 512-bit vector ALUs within the 0.15 mm? per-unit budget. If not, BEMI cannot efficiently run AVX-512 code.
2. Profile real HPC workloads to determine opmask usage frequency. If opmask is rare (most AVX-512 code uses masked operations sparingly), the 1.5x-2.0x case is rare.
3. Consider an "AVX-512 mode" where select back-end units are widened to 512-bit at the expense of unit count -- a dynamic trade-off.

## Current Status

**Unresolved.** The published doc mentions AVX-512 in passing (Chapter 4.5) but does not address the translation cost in Weaponized mode. This is a gap.

