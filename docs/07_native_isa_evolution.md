# 07. Bemi v1.1 -- Native ISA Evolution

> **Architecture version:** Bemi v1.1 | Predecessor: [Bemi v1.0 (Chapter 03)](03_hybrid_bemi_first_implementation.md) | Successor: [Bemi v1.2 (Chapter 08)](08_weaponized_x86_bemi.md) | ROB Density variant: [Bemi v1.3](08_weaponized_x86_bemi.md#86-bemi-v13-rob-entry-density-derivation)
> v1.1 removes the x86 decoder entirely (1-cyc decode, 36 threads, 65W). v1.3 achieves 84 threads via ROB entry size reduction. For full comparison see [Chapter 14](14_architecture_version_comparison.md).

## 7.1 The Remaining Problems with Dynamic Binary Translation

The Macro-Op Passthrough solved the ASIC hardware problem. But the `hybrid_bemi` translator
remained a **software Dynamic Binary Translation (DBT)** layer -- a runtime system that intercepts
x86 instructions and converts them to Bemi micro-ops on the fly.

Even with passthrough working perfectly, three fundamental microarchitectural problems remained
unsolvable within a software DBT framework. These are documented in `docs/04_native_isa_evolution.md`.

---

## 7.2 Problem 1: Total Store Ordering (TSO)

x86 enforces a strict memory consistency model called **Total Store Ordering (TSO)**. Under TSO:

1. All stores are visible to other cores in program order (no store reordering)
2. Loads may bypass older stores in the store buffer (but only to *other* addresses)
3. Reads of the same address always return the most recently written value

This model is what allows x86 multi-threaded programs to work correctly without explicit memory
barrier instructions in most cases. C++ programs compiled for x86 rely on TSO guarantees implicitly.

**The DBT Problem:**
A pure RISC architecture (like ARM or RISC-V) uses a *relaxed* memory model. Stores can be
reordered relative to each other. Loads can be reordered relative to stores.

If Bemi runs an x86 program on a relaxed RISC memory model, **multi-threaded programs will
produce incorrect results** -- because the x86 compiler assumed TSO guarantees that the relaxed
model doesn't provide.

The DBT layer must compensate by inserting **memory barrier (fence) instructions** at every
store that might be visible to other threads. This adds approximately **15 additional cycles**
per atomic or shared-memory store operation.

**The Benchmark Impact:**
In `tests/tso_concurrency_bench.py`, the "Hybrid Bemi (Software TSO)" configuration shows:

```
Effective Latency: 46 cycles/op  (vs 26 for x86, vs 31 for Bemi Native)
Throughput: 0.783  (worse than x86's 0.923)
Exec Time: 6,388,888  (worst of all three)
```

The 15-cycle software TSO fence converts a thread-density advantage into a throughput *loss*.
Hybrid Bemi loses to both native x86 and native Bemi on concurrent atomic workloads.

---

## 7.3 Problem 2: Self-Modifying Code

JIT compilers -- V8 (JavaScript), HotSpot (Java JVM), .NET CLR, and game engine scripting layers --
constantly **write new machine code into memory and then execute it**.

This is called self-modifying code (SMC). It is one of the most powerful techniques in
high-performance runtime systems, but it is catastrophic for DBT layers.

**The DBT Problem:**
When a DBT layer like `hybrid_bemi` translates a block of x86 code, it:
1. Reads the original x86 bytes from memory
2. Generates a Bemi micro-op sequence
3. Caches the translation (this is the "trace cache")

But if the JIT compiler writes *new* x86 bytes over the same memory region (self-modification),
the cached translation in the DBT trace cache is now **stale**. The DBT layer must:

1. Detect the write (by marking pages as write-protected and taking a page fault)
2. Invalidate the stale trace-cache entry
3. Re-translate the new x86 bytes on the next execution

This invalidation cycle introduces unpredictable **latency spikes** -- the exact time a JIT-heavy
application calls the most performance-critical function might be the time the DBT layer is
flushing and re-translating its trace cache.

**Real-world impact:** Running Node.js, a Java application, or a game engine (Lua scripting)
on a software DBT layer like Rosetta 2 or `hybrid_bemi` will exhibit intermittent pauses
that are absent in native execution.

---

## 7.4 Problem 3: Indirect Branches

Many x86 programs use **indirect branches** -- jumps where the target address is stored in a
register, not hardcoded in the instruction:

```asm
JMP RAX          ; jump to the address stored in register RAX
CALL [RBX + 8]  ; call the function pointer at memory address RBX+8
```

Indirect branches are ubiquitous in:
- C++ virtual method dispatch (`vtable` calls)
- Function pointers
- Switch statements (often compiled as jump tables)
- Dynamic library calls

**The DBT Problem:**
In native x86, the CPU's Branch Target Buffer (BTB) learns the likely targets of indirect
branches over time. After a few iterations, `JMP RAX` is predicted with high accuracy.

In a DBT layer, the target address in `RAX` is an *x86 address* -- but the translated code
lives at a *different* Bemi address. The DBT layer must maintain a **hash table** mapping
x86 addresses to Bemi addresses. Every indirect branch requires a hash-table lookup:

1. Read `RAX` (1-2 cycles)
2. Hash the value (1-2 cycles)
3. Look up in the translation table (L1 cache hit: 4 cycles, miss: 40+ cycles)
4. Jump to the Bemi address

This adds **10-40 cycles** to every indirect branch. For OOP-heavy code (like C++ with virtual
methods), indirect branches can account for 20-30% of all branches -- making this penalty
extremely costly.

**The Benchmark Impact:**
In `tests/branch_prediction_bench.py`, "Hybrid Bemi (Software DBT)" shows:

```
Indirect penalty multiplier: 4.0x  (vs x86's 1.2x)
Total cycles: 18,000,000  (only slightly better than x86's 19,280,000)
Penalty overhead: 44.4%  (nearly as bad as x86's 48.1%)
```

The DBT hash-table lookup essentially eliminates Bemi's branch-penalty advantage (8-cycle pipeline
vs x86's 16-cycle pipeline) on indirect branches.

---

## 7.5 The Solution: Native Compiler Co-Design

All three problems share a common root cause: **the translation is happening at runtime**, which
means the translated code runs in an environment that was designed for x86.

The solution is to push the translation to **compile time**:

> Instead of trapping and translating x86 bytes at runtime, translate once with an LLVM backend
> and distribute native Bemi binaries.

### How This Solves Each Problem

**TSO:** The compiler can insert TSO fence instructions *precisely where the x86 memory model
requires them*, based on static analysis. Runtime performance isn't impacted by fence-insertion
overhead -- the compiler handles it once, ahead of time. The hardware can then enforce TSO
natively in the out-of-order engine (just as ARM's TSO mode does).

**Self-Modifying Code:** Programs compiled to native Bemi ISA don't need DBT trace caches.
There is nothing to invalidate. JIT-compiled code (V8, HotSpot) compiles directly to Bemi
micro-ops instead of x86 bytes -- the JIT compiler simply has a Bemi backend.

**Indirect Branches:** In native Bemi binaries, indirect branch targets *are* Bemi addresses.
There is no translation table. The BTB learns Bemi addresses directly. Indirect branch
predictions are just as accurate as on native x86.

Additionally, the Bemi BIOS's Ring -1 DBT **pre-loads** the TAGE branch predictor tables with
the translated targets at boot time for the legacy OS kernel. This is why the Weaponized Bemi
configuration shows an **indirect penalty multiplier of 0.8** (better than x86's 1.2x) in the
branch prediction benchmark.

---

## 7.6 The Native ISA Specification

The Native Bemi ISA is defined by these properties:

**Front-end:**
- All instructions are exactly 32 bits (4 bytes) wide -- no exceptions
- Decode latency: 1 clock cycle (read 4 bytes, produce routing signal)
- Decoder is stateless -- no variable-length scan, no prefix accumulation

**Back-end (inherited from x86):**
- Out-of-order execution with ROB and register renaming
- Hardware TSO enforcement (native, not emulated)
- TAGE branch predictor
- Standard BTB with full indirect target caching
- Same L1/L2/L3 cache hierarchy

**Compiler target:**
- LLVM backend generates Bemi micro-ops
- AOT compilation produces static Bemi ELF binaries
- ABI is compatible with standard ELF calling conventions (adjusted for Bemi register names)
- JIT compilers (V8, HotSpot) add a Bemi code generation backend

This design is the "best of both worlds": x86 back-end power and robustness, RISC front-end
simplicity and density.

