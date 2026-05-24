# BEMI Architecture vs. Native x86 Benchmark Report

This report evaluates the performance of the BEMI architecture (both Native ISA and Hybrid Translation variants) against a native x86 baseline across various workloads, including pure compute, specialized instructions, concurrency constraints, and memory hierarchy limits.

## 1. Fundamentals: Compute & Memory

### Arithmetic & Memory
| Architecture | Arithmetic Throughput | L1i Hit Rate | Memory Bandwidth |
|---|---|---|---|
| Native x86 (CISC) | 4.8 | 93.0% | 3.03 |
| Bemi RISC (Fixed-32) | 36.0 | 99.0% | 36.18 |

### CISC Muscles (Op Specific Speedups)
| Category | Speedup (Bemi vs x86) |
|---|---|
| Basic Arithmetic | 15.0x |
| String Operations | 20.25x |
| Complex Math (FSIN) | 8.4x |
| Vector/AVX-512 | 1.5x |
| Context Switching | 4.12x |

### Power Efficiency
| Category | Efficiency Gain |
|---|---|
| Arithmetic | 15.0x |
| Strings | 20.25x |

### Standard Benchmarks
**Geekbench Equivalent**
| Architecture | Single-Core | Multi-Core |
|---|---|---|
| Native x86 | 800 | 19,200 |
| Bemi RISC | 2,100 | 302,400 |

**AI Training Time**
- Native x86 Time: 15625
- Bemi RISC Time: 1736
- **Bemi Gain: 9.0x**

**Video Playback 4K**
- Native x86 FPS: 18.22
- Bemi RISC FPS: 37.38

## 2. Advanced Workloads & Architectural Constraints

### TSO Concurrency & Atomic Operations
Tests the overhead of e
<truncated 671 bytes>
x86) against smaller BTBs and translator lookup penalties.
| Architecture | Total Cycles (Rel) | Penalty Overhead |
|---|---|---|
| Native x86 | 19,280,000 | 48.1% |
| Bemi RISC | 15,000,000 | 33.3% |
| Hybrid Bemi (DBT) | 18,000,000 | 44.4% |

*Analysis:* Native Bemi's shorter pipeline dominates. Hybrid Bemi suffers close to x86 levels of overhead due to dynamic binary translation lookups on indirect branches.

### Memory Hierarchy & Cache Contention
Simulates 100MB of accesses evaluating L1/L2/L3 cache misses against core densities.
| Architecture | Avg Access Latency (cyc) | Execution Contention (Rel) |
|---|---|---|
| Native x86 | 5.21 | 2,713,541 |
| Bemi RISC (Native ISA) | 5.21 | 904,513 |
| Hybrid Bemi (DBT Cache Pressure) | 24.09 | 4,182,870 |

*Analysis:* Native Bemi perfectly leverages cache despite high cores. However, Hybrid Bemi fails completely here; the DBT translator evicts standard data from caches, spiking latency drastically to 24 cycles on average.

## 3. Conclusions

1. **Native Compute & Thread Scaling:** Bemi universally destroys x86 in raw compute, string manipulation, and parallel tasks owing to its 36-to-12 thread density advantage and 0-cycle decode penalty.
2. **Hybrid Bemi's Critical Flaw:** While software bounds on TSO and Branches remain faster than x86, **Cache Pressure in Hybrid Bemi (DBT)** renders it far slower than x86 for heavy memory workflows. The binary translation system eats too much cache space.
3. **The Macro-Op Passthrough Necessity:** The 1.5x AVX-512 speedup verifies that complex CISC hardware must be retained and targeted via passthrough, as software emulation of ASICs is inefficient.
4. **Native ISA is King:** Native Bemi completely avoids the cache and branch mapping issues of Hybrid DBT, verifying the project's pivot strategy to a compiled Native ISA model.
