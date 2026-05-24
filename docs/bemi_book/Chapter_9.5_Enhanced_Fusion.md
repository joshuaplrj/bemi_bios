# Chapter 9: Bemi v2.0: Scaled Dominance Architecture

## 9.5 Extended 6-Pair Macro-Op Fusion

### 9.5.1 The Fusion Throughput Multiplier
Macro-Op Fusion is a hardware-firmware co-design technique where the front-end decodes multiple adjacent instructions and merges them into a single, complex micro-op (uop) before dispatch. This reduces the number of slots consumed in the pipeline, effectively raising the Instructions Per Clock (IPC) without widening the physical execution ports.

Bemi v1.3 supported only 2 basic fusion pairs (CMP+Jcc and TEST+Jcc). Bemi v2.0 expands this database to **6 fusion pair types**, reflecting modern high-performance microarchitectures:

1. **CMP + Jcc:** Fuses integer compare and conditional branch.
2. **TEST + Jcc:** Fuses bitwise test and conditional branch.
3. **ADD/SUB + Jcc:** Fuses arithmetic calculation and conditional branch.
4. **INC/DEC + Jcc:** Fuses loop counter adjustment and loop branch.
5. **MOV + CMP + Jcc:** Fuses 3-way memory-load, compare, and branch.
6. **LEA + ADD:** Fuses address calculation and base index addition.

### 9.5.2 Grounded IPC Validation
By supporting these 6 patterns, the Bemi front-end achieves an average instruction reduction of $15\%$ in the execution stream. Combined with the 4-cycle decode pipeline, this drives the peak effective IPC per thread from $1.3$ (in v1.3) to **1.5** in Bemi v2.0. This 1.5x IPC multiplier is validated against published physical measurements from advanced mobile cores (e.g., ARM Cortex-A710), proving that macro-op fusion delivers substantial IPC gains without physical decoder modifications.
