# 02. CISC Instruction Taxonomy

## 2.1 Why Categorisation Matters

Not all x86 instructions are equal. Before Bemi can handle x86 code, the project must answer
a precise question: **what categories of instructions exist, and what does each category cost?**

A naive RISC translation layer treats every x86 instruction as a generic blob to be decomposed.
This approach fails catastrophically on workloads that rely on x86's specialised hardware ASICs
(like AES-NI or AVX-512). Bemi's engineering breakthrough -- the Macro-Op Passthrough -- was only
possible because the team first built a complete taxonomy of what they were dealing with.

The source document for this analysis is `86'bench.md` in the project root.

---

## 2.2 Category 1: Microcoded Heavyweights

These are x86 instructions so complex that the hardware decoder cannot process them in its
normal pipeline. When the CPU encounters one of these opcodes, it hands control to the
**Microcode Sequencer (MSROM)** -- a dedicated ROM that generates a large stream of internal
micro-ops to implement the instruction's behaviour.

### Representative Instructions

**`REP MOVSB` (Block Memory Copy)**
The instruction tells the CPU: "Copy `RCX` bytes from the address in `RSI` to the address in `RDI`
and keep looping until `RCX` reaches zero." On a RISC architecture, this is implemented as an
explicit software loop with Load, Store, Decrement, and Branch instructions. In x86, it is a
*single* opcode that engages the ERMS (Enhanced REP MOVSB) microcode engine.

Modern ERMS microcode dynamically optimises the copy based on `RCX` and memory alignment:
for large aligned copies it switches to 256-bit or 512-bit internal data paths, bypassing
the architectural register file entirely for maximum throughput. This is fundamentally
impossible to replicate in software without accessing hardware-private micro-op sequences.

**Bemi Challenge:** Without the Macro-Op Passthrough, a RISC emulation of `REP MOVSB` requires
approximately **6 instructions per byte** (loop setup + load + store + increment + compare + branch).
This is an 8x to 50x expansion depending on data size. However, the RISC loop executes at 1 cycle
per instruction (no CISC decode penalty), so the expansion is partially absorbed.

**`FSIN` / `FCOS` / `FSQRT` (Transcendental Math)**
The x87 FPU provides native hardware instructions for computing sines, cosines, and square roots.
These map to dedicated microcode sequences that use iterative polynomial approximation hardware.
On pure RISC without equivalent silicon, computing `FSIN` requires a software Taylor series
approximation (typically 20-30 floating-point multiply-add operations).

**Bemi Challenge:** `FSIN` in software RISC requires approximately **30 basic instructions**
compared to a single microcoded x86 instruction with ~80 cycles of dedicated hardware execution.
The RISC software path at 1 cycle/instruction (30 instructions) is actually faster than the
microcoded hardware path (80 cycles) -- which is why Bemi beats x86 on FSIN even without passthrough.

**`XSAVE` / `XRSTOR` (Process State Save/Restore)**
A single instruction that dumps the entire processor state (all registers, SSE/AVX state,
floating-point state) to a memory buffer. Used during operating system context switches.

---

## 2.3 Category 2: SIMD Vector Extensions (AVX/AVX-512)

This is x86's most powerful advantage over generic RISC architectures. Intel and AMD added
instructions that perform the same operation on **multiple pieces of data simultaneously** --
Single Instruction Multiple Data (SIMD).

### The Scale of AVX-512

`VFMADD213PS zmm0, zmm1, zmm2` (Fused Multiply-Add on 512-bit packed singles):
- Performs 16 floating-point multiply-add operations **simultaneously**
- Produces 16 results in a single instruction
- Uses a 512-bit dedicated FMA execution unit that takes **4 clock cycles**

**Bemi Challenge Without Passthrough:**
To emulate this in pure RISC, each of the 16 float operations must be performed individually:
16 FP loads + 16 FP multiplies + 16 FP adds + 16 stores = **64 RISC instructions**.
Even at 1 cycle each, 64 cycles on 36 threads versus 4 cycles on 24 threads -- x86 wins.

**Bemi Challenge With Passthrough:**
The Macro-Op Passthrough routes the Bemi Macro-Op directly to the host's 512-bit FMA hardware.
Bemi pays 1 decode cycle instead of 4. The execute phase (4 cycles on the same hardware) is
identical. Net result: Bemi wins through decode savings plus 1.5x thread advantage.

### Why SIMD is the Key Battleground

SIMD is the area where the CISC-vs-RISC debate is most consequential. A pure RISC processor
without vector extensions is simply unable to match x86 throughput on vectorised scientific
computing, video encoding, and AI inference workloads. The Macro-Op Passthrough is Bemi's
answer to this structural disadvantage.

---

## 2.4 Category 3: Hardware & Privilege Management

x86 was designed to run operating systems at the silicon level. This means the ISA has
native built-in instructions for managing the deepest hardware privilege levels.

### Virtualization Instructions (`VMLAUNCH`, `VMRUN`)

These instructions spin up hardware-isolated virtual machines at Ring -2 (VMX root mode),
below even the operating system's Ring 0. They are fundamental to modern cloud computing --
every AWS, Azure, and GCP VM runs on top of these instructions.

**Bemi Relevance for BIOS:** The Bemi BIOS's Ring -1 DBT hypervisor is itself implemented
on top of these mechanisms. The BIOS doesn't need to *emulate* `VMLAUNCH` -- it uses it as
infrastructure to host the translation layer beneath OS visibility.

### Cache Management Instructions (`INVLPG`, `CLFLUSH`)

These allow software to surgically control the CPU's L1/L2/L3 caches:
- `INVLPG`: Invalidates a specific TLB (Translation Lookaside Buffer) entry.
- `CLFLUSH`: Forces a specific cache line to be written back to memory.

**Bemi BIOS Relevance:** The Bemi BIOS must correctly pass through these instructions when the
guest legacy OS uses them. A misbehaving cache invalidation can corrupt the trace cache that
the Ring -1 DBT layer relies on.

### Interrupt Instructions (`INT`, `IRET`)

`INT n` executes a software interrupt:
1. Pushes FLAGS, CS, and IP onto the stack (3 memory writes)
2. Clears IF and TF flags
3. Reads the interrupt vector table (IVT) at address `n * 4` (1 memory read)
4. Loads the new CS:IP from the IVT entry (2 register writes)
**Total: 51 clock cycles on the 8086 (documented hardware cost)**

`IRET` reverses the process on interrupt return.

**This 51-cycle cost is the central number in the MS-DOS 1.0 BIOS benchmark** (Chapter 12).
Every system call in MS-DOS 1.0 is an `INT 21h`. Every hardware event is an `INT 8h`.
The Bemi BIOS's value proposition for legacy OS hosting comes entirely from eliminating this
51-cycle overhead by replacing it with an 8-cycle trace-cache hit.

---

## 2.5 Category 4: Cryptography (AES-NI)

`AESENC xmm1, xmm2` performs one complete round of AES encryption on 128 bits of data.
Internally, it executes the SubBytes, ShiftRows, MixColumns, and AddRoundKey transformations --
the four fundamental operations of the AES algorithm -- in dedicated ASIC silicon in **4 clock cycles**.

A software implementation of AES requires table lookups (S-box), XOR operations, and byte
shuffling. Even heavily optimised software AES runs at 200-500 cycles per 128-bit block.

**Bemi Status:** AES-NI is a prime candidate for the Macro-Op Passthrough. The passthrough
routes a single Bemi Macro-Op to the same AESENC ASIC. Bemi pays 1 decode cycle instead of 4.
The ASIC executes in the same 4 cycles. The net result is:
- x86: `4 decode + 4 execute = 8 cycles`
- Bemi (passthrough): `1 decode + 4 execute = 5 cycles`
- At 36 threads vs 24 threads, Bemi delivers ~2.4x throughput improvement on AES workloads.

---

## 2.6 Summary Table: Bemi's Approach by Category

| Category | Representative Instructions | Bemi Strategy | Bemi Outcome |
|---|---|---|---|
| Microcoded Heavyweights | REP MOVSB, XSAVE | Passthrough (ERMS) or RISC loop | Win on MOVSB; expansion hurts on XSAVE |
| Transcendental Math | FSIN, FCOS | RISC software approximation | Win (RISC Taylor series faster than 80-cycle HW) |
| SIMD / AVX-512 | VFMADD213PS, VADDPS | Macro-Op Passthrough to FMA ALU | Win with passthrough; lose without |
| AES Cryptography | AESENC, AESDEC | Macro-Op Passthrough to ASIC | Win with passthrough; lose without |
| Privilege / BIOS | VMLAUNCH, INT, IRET | Ring -1 BIOS intercept + trace cache | Win (51-cycle INT -> 8-cycle cache hit) |
| Cache Management | INVLPG, CLFLUSH | Pass-through transparent | Neutral (same silicon, no overhead) |

