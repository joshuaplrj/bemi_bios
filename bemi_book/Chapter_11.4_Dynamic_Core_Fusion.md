# Chapter 11: Bemi v4.0: Ultra-Bandwidth & Execution Zenith Architecture

## 11.4 Dynamic Core/Thread Fusion (DCF)

### 11.4.1 Resolving the Amdahl's Law serial phase bottleneck
While Bemi's 72-thread SMT architecture delivers massive throughput during highly parallel phases of computation, it faces a physical limitation during serial phases. According to **Amdahl's Law**, the execution speedup of any workload is strictly limited by its sequential fraction:
$$ \text{Speedup} = \frac{1}{(1 - P) + \frac{P}{S}} $$
Where $P$ is the parallel fraction and $S$ is the parallel speedup. During serial phases ($P \to 0$), the execution speedup is bounded by the performance of a single thread. If a single thread is restricted to a small 313-entry ROB and must execute scalar instructions, the overall workload stalls.

### 11.4.2 The Core Fusion Control Logic
To resolve this, Bemi v4.0 implements **Dynamic Core/Thread Fusion (DCF)**. 

When the hardware scheduler detects that a core's workload has transitioned into a serial execution phase (indicated by thread stalls, low SMT activity, or high branch density in a single thread), it dynamically commands adjacent SMT threads to fuse their physical resources.

```
  Normal SMT-6 Mode:       [ Thread 1: 313 ROB ] [ Thread 2: 313 ROB ] [ Thread 3: 313 ROB ]
  
  Fused "Super-Thread" Mode: [============= Fused Thread A: 626 ROB =============] [ T3: 313 ]
```

- **Fused Mode:** The 6 SMT threads on a core fuse into 3 "Super-Threads" (36 total logical threads across the chip).
- **ROB Aggregation:** The private ROB banks of adjacent threads are aggregated, providing the active fused thread with a massive **626-entry ROB window**.
- **Memory-Level Parallelism:** The aggregated execution resources enable **MLP-12** (overlapping up to 12 cache misses).
- **Effective Latency Hiding:**
  $$ \text{Latency}_{\text{effective}} = \frac{\text{Latency}_{\text{blended}}}{\text{MLP}} = \frac{20 \text{ cycles}}{12.0} = 1.67 \text{ cycles} $$

By reducing effective memory latency to a blistering **1.67 cycles** during serial phases, DCF allows Bemi v4.0 to resolve sequential bottlenecks natively in hardware, enabling heavy serial workloads like ray tracing and garbage collection to scale cleanly.
