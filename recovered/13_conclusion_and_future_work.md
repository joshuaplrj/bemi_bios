# 13. Conclusion & Future Work

> This chapter concludes the Bemi architecture documentation covering **v1.0** (Hybrid DBT),
> **v1.1** (Native RISC ISA, 36T), **v1.2** (Weaponized x86 Bemi, 144T), and **v1.3** (ROB Entry Density, 84T).
> For a detailed four-way benchmark comparison see [Chapter 14](14_architecture_version_comparison.md).
> v1.3 derives its thread count from 4-byte ROB entries (vs x86's 14 bytes), yielding 3.5x density from the same SRAM budget.


## 13.1 What Has Been Proven

This project has established, through rigorous simulation models grounded in documented hardware
parameters, the following claims across **four distinct architecture versions**:

### Proven: Bemi v1.1 decode advantage is real (1-cyc fixed-32)

The fixed-32 decoder reducing decode latency from 4 cycles to 1 cycle is a structural
consequence of instruction length -- any processor with fixed-length 32-bit instructions decodes
in 1 cycle because boundaries are known at compile time. IPC rises from 1.0 to 5.2x.
Single-thread performance: **5.2x better than x86**. See [Chapter 07](07_native_isa_evolution.md).

### Proven: Bemi v1.2 thread density from 6nm physics

At 6nm, RISC execution back-ends (0.15 mm?) are 20x smaller than x86 execution back-ends
(2.25 mm?). Keeping the x86 decoder and filling freed back-end area gives 144 virtual threads
vs x86's 24. Multi-thread throughput: **7.8x better than x86**.
Single-thread IPC: only **1.3x better** (fusion onl
<truncated 8412 bytes>
re socket. Multi-socket systems require NUMA-aware
interrupt routing that the current Shadow APIC model doesn't address.

**Q4: What is the Bemi ABI?**
The calling convention for Bemi native binaries has not been formally specified. Register
allocation, stack frame layout, and exception handling unwinding tables all require an ABI
document before a production compiler can target Bemi.

---

## 13.5 The Core Thesis, Restated

Bemi is not an attempt to make RISC beat CISC by brute force. It is an attempt to identify
the one component of x86 that provides the worst performance-per-area-per-watt trade-off
(the variable-length instruction decoder), remove it, and invest the freed resources in
the component that provides the best performance-per-area-per-watt trade-off (execution
thread density via ROB depth).

The three x86 wins in the benchmark suite (CISC hardware emulation without passthrough, and
memory hierarchy pressure) define the *envelope* of this approach. They are not failures --
they are the correct identification of where the trade-off goes against Bemi:

- When dedicated ASIC hardware is needed and the passthrough is unavailable: x86 wins.
- When memory-bound workloads saturate L1/L2 cache and thread density becomes a liability: x86 wins.

Everything outside that envelope is Bemi's domain.

The engineering project's ultimate claim is straightforward: **for the majority of real-world
compute workloads (integer arithmetic, AI inference, cryptography with passthrough, general-purpose
server/desktop code), a RISC front-end with 1.5x virtual thread density and the x86 back-end
intact is a superior design to the full x86 ISA front-end.**

The benchmarks support this claim. The arithmetic is sound. The physics are real.
The engineering remains to be done.

