# Chapter 9: Bemi v2.0: Scaled Dominance Architecture

## 9.4 Memory-Level Parallelism (MLP) Engine

### 9.4.1 Hiding DRAM Latency via Out-of-Order Execution
When an instruction suffers an L1 cache miss, the CPU must fetch the data from physical DRAM, which requires approximately 40 cycles. If the processor can only execute instructions sequentially, the pipeline stalls immediately, yielding an effective performance of zero during the DRAM roundtrip.

To combat this, out-of-order execution engines search the ROB for independent instructions to execute while the memory request is pending. **Memory-Level Parallelism (MLP)** refers to the CPU's ability to overlap multiple outstanding memory misses simultaneously.

```
Time (Cycles) --->
Thread A:  [ Miss 1: DRAM Read (40 cycles) -----------------------------------> ]
           [       Miss 2: DRAM Read (40 cycles) -----------------------------> ]
           [              Miss 3: DRAM Read (40 cycles) -----------------------> ]
           |<- Overlapped Memory Fetching: Latency is hidden by MLP-6 ->|
```

### 9.4.2 Quantifying Bemi's MLP Edge
In native x86 architectures, each SMT thread's out-of-order window is capped by its shared ROB portion (typically 112 entries). This restricts the maximum MLP to approximately 3.5.

In Bemi v2.0, the dedicated **196-entry ROB bank** per thread provides a much larger search window. The Bemi v2.0 hardware engine achieves an average **MLP of 6**, allowing up to 6 cache misses to be issued to the memory controller concurrently.

**Effective Memory Latency calculation:**
$$ \text{Latency}_{\text{effective}} = \frac{\text{Physical Latency}}{\text{MLP}} $$
- **x86 Baseline:** $\frac{40 \text{ cycles}}{3.5} = 11.43 \text{ cycles}$
- **Bemi v2.0:** $\frac{40 \text{ cycles}}{6.0} = 6.67 \text{ cycles}$

By overlapping cache misses, Bemi v2.0 effectively cuts the penalty of DRAM access in half, hiding memory latency within the out-of-order execution window.
