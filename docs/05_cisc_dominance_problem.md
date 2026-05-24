# 05. The CISC Dominance Problem

## 5.1 The Catastrophic Failure of Pure RISC

After the `hybrid_bemi` prototype proved that RISC translation could beat x86 on general integer
workloads, the team ran a critical control experiment: **what happens when Bemi faces hardware-
accelerated x86 instructions?**

The results were stark. This is documented in `tests/cisc_dominance.py` -- which exists specifically
as an *honest control benchmark* showing where Bemi loses.

> **v1.3 cross-reference:** The ROB Entry Density model (84 threads, 109.2 TP) reduces the thread
> count from v1.2's 144, which changes the CISC dominance calculations. For example, on AVX-512
> without passthrough: v1.3 = (4+64)/109.2 = 0.623 ticks vs x86 = 0.333 ticks (1.87x loss).
> See [Chapter 14](14_architecture_version_comparison.md) for the full v1.3 comparison.

---

## 5.2 The AVX-512 Problem: 16x Instruction Expansion

When Bemi encounters `VFMADD213PS zmm0, zmm1, zmm2` (fused multiply-add on 16 floats), it cannot
use the dedicated 512-bit FMA hardware -- because a pure RISC software translator doesn't have
a mechanism to route to ASIC silicon. Instead, it must emulate the operation in software.

The software emulation of 16-float FMA in RISC:
```
16x LOAD instructions   (load each float from zmm registers)
16x FMUL instructions   (multiply each pair)
16x FADD instructions   (add the third vector)
16x STORE instructions  (write results)
= 64 RISC instructions total
```

Each RISC instruction executes in 1 cycle with the fixed-32 decoder.
Total: **64 cycles of Bemi execution**.

Compare to x86:
- Decode: 4 cycles (complex decoder for AVX-512)
- Execute: 4 cycles (dedicated 512-bit FMA port)
- Total: **8 cycles of x86 execution**

**Wall-clock comparison (ticks = cycles / threads):**
- x86: `8 / 24 = 0.333 ticks`
- Bemi (no passthrough): `(1 + 64) / 36 = 1.806 ticks`

x86 is **5.4x faster** on AVX-512 without the passthrough. This is a decisive CISC win.

---

## 5.3 The AES-NI Problem: 120x Instruction Expansion

The AES encryption case is even worse. `AESENC xmm1, xmm2` performs:
- SubBytes: 16 S-box table lookups (non-linear substitution)
- ShiftRows: 16 byte-level rotations
- MixColumns: 4 Galois Field polynomial multiplications
- AddRoundKey: 16 XOR operations

This is a mathematically complex operation that the AESENC ASIC computes in **4 clock cycles**
using dedicated wiring. In RISC software:

```
SubBytes  : 16 table lookups (index + load) = ~32 instructions
ShiftRows : 16 byte extractions + shifts = ~32 instructions
MixColumns: 4 GF(2^8) multiplications (each ~8-10 ops) = ~36 instructions
AddRoundKey: 16 XORs = 16 instructions
= ~116-120 RISC instructions
```

**Wall-clock comparison:**
- x86: `(4 + 4) / 24 = 0.333 ticks`
- Bemi (no passthrough): `(1 + 120) / 36 = 3.361 ticks`

x86 is **10.1x faster** on AES without the passthrough. This is an overwhelming CISC win.

---

## 5.4 The REP MOVSB Situation: Bemi Surprisingly Wins

Counter-intuitively, Bemi *beats* x86 on REP MOVSB string copy even without the passthrough.
Here is why:

`REP MOVSB` routes through the Microcode Sequencer (MSROM) in x86. Even with ERMS optimisation,
the CPU spends cycles setting up the microcode sequence before the actual copies begin.
The execution model for `REP MOVSB` in x86 is approximately **2 cycles per copy iteration**
(after ERMS setup overhead), plus **4 cycles of decode**.

In Bemi's RISC emulation, a simple loop:
```
LOAD RTmp, [RSI + offset]
STORE [RDI + offset], RTmp
INC offset
CMP offset, RCX
JMP (if not done)
```
= **5 instructions per element** (not 6, since we can use address post-increment).

**Wall-clock comparison:**
- x86: `(4 + 2) / 24 = 0.250 ticks`
- Bemi (no passthrough): `(1 + 5) / 36 = 0.167 ticks`

Bemi is **~1.5x faster** on string copy even in pure RISC mode. This is why `cisc_dominance.py`
shows Bemi winning on MOVSB -- the expansion factor (5-6x) is less harmful than the ERMS microcode
overhead combined with x86's 4-cycle decoder.

---

## 5.5 The cisc_dominance.py Results (Corrected)

The corrected benchmark (`tests/cisc_dominance.py`) with honest thread counts (24 vs 36):

| Workload | x86 Ticks | Bemi Ticks | Winner |
|---|---|---|---|
| AVX-512 (Vector Math) | 0.3333 | 1.8056 | x86 (5.4x) |
| AES-NI (Crypto) | 0.3333 | 3.3611 | x86 (10.1x) |
| REP MOVSB (String Copy) | 0.2500 | 0.1944 | Bemi (1.3x) |

This table is the **honest control case** that demonstrates the limits of pure RISC emulation.
The two x86 wins here are real and cannot be wished away -- they motivated the Macro-Op Passthrough.

---

## 5.6 The cisc_muscles.py Results (Broader Workload Survey)

`tests/cisc_muscles.py` extends the analysis to five categories:

| Category | x86 Time (rel) | Bemi Time (rel) | Winner |
|---|---|---|---|
| Basic Arithmetic | 208,333 | 83,333 | Bemi (2.5x) |
| String Operations | 2,250,000 | 444,444 | Bemi (5.1x) |
| Complex Math (FSIN) | 3,500,000 | 1,666,666 | Bemi (2.1x) |
| Vector/AVX-512 | 333,333 | 888,888 | x86 (2.7x) |
| Context Switching | 1,833,333 | 1,777,777 | Bemi (1.03x) |

**Key observations:**

1. **FSIN (Complex Math):** Bemi wins despite 30x instruction expansion. The reason: FSIN
   executes in 80 cycles on dedicated x86 hardware, which at 24 threads gives `(4+80)/24 = 3.5`.
   Bemi's RISC Taylor series requires 30 ops at 1 cycle each: `(1+30)/36 = 0.86`. Bemi is faster.
   
2. **Context Switching:** Bemi barely wins (1.03x). Context switching requires saving 32+ registers
   (`XSAVE` equivalent) and updating the TLB. RISC expansion is ~32x but thread density and
   1-cycle decode nearly break even with x86's 40-cycle execution complexity.

3. **Vector/AVX-512:** x86 dominates (2.7x) -- confirming the earlier analysis. The RISC software
   loop cannot compete with dedicated 512-bit FMA hardware.

---

## 5.7 The Engineering Conclusion

Pure RISC translation works for arithmetic, strings, and even complex math.
But it **fails on dedicated hardware ASICs** -- specifically AVX-512 and AES-NI.

This is not a fundamental flaw in RISC. It is a specific failure of *software emulation of hardware*.
The insight that led to the Macro-Op Passthrough is:

> **"Bemi doesn't need to emulate the ASIC. It just needs to use it."**

If Bemi can route its fixed-32 Macro-Op directly to the same 512-bit FMA unit or AESENC ASIC
that x86 uses, then:
- Bemi pays its 1-cycle fixed decode (instead of x86's 4-cycle complex decode)
- The ASIC executes in the same 4 cycles for both architectures
- Bemi's 1.5x thread advantage compounds on top

The result is an "Architecture Inversion" -- the exact workloads where Bemi was losing the most
become workloads where Bemi wins the most. This breakthrough is documented in Chapter 06.

