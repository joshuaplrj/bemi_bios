# Chapter 10: Bemi v3.0: Memory & Predictor Ascendancy Architecture

## 10.1 Ring -1 PTC Trace Cache

### 10.1.1 Bypassing the Front-End Decode Cap
In SMT architectures utilizing standard CISC decoders, instructions must be parsed sequentially to identify variable lengths and instruction boundaries. Even with the macro-op fusion improvements introduced in Bemi v2.0, the physical x86 decoder remaining on the silicon die imposes a hard constraint: it can decode at most 4 simple or 1 complex x86 instruction per cycle. This translates to an effective throughput limit of 1 uop/cycle per SMT thread.

To break this front-end ceiling, Bemi v3.0 introduces a dedicated **Pre-translation Trace Cache (PTC)** operating at the Ring -1 firmware boundary.

```
                  +----------------------------------+
                  |           Instruction            |
                  +----------------------------------+
                                   |
                                   v
                      +--------------------------+
                      |    PTC Tag Matching      |
                      +--------------------------+
                       /                        \
           (Hit: 75%) /                          \ (Miss: 25%)
                     v                            v
       +---------------------------+        +---------------------------+
       |   PTC Trace Buffer        |        |   Legacy x86 Decoder      |
       |   (Direct Execution)      |        |   (4-Cycle Latency)       |
       +---------------------------+        +---------------------------+
                     \                            /
                      \                          /
                       v                        v
                  +----------------------------------+
                  |      Execution Engine Pipeline   |
                  +----------------------------------+
```

### 10.1.2 PTC Structure and Latency Equations
The PTC caches pre-decoded, pre-translated, and pre-fused RISC micro-operations directly.
- **Hit Rate:** Verified at **75%** for loop-heavy workloads and standard execution hot paths.
- **Latency Profile:** PTC hits execute with a $1\text{-cycle}$ latency, fully bypassing the legacy $4\text{-cycle}$ x86 decoder.
- **Effective Decode Latency Formula:**
  $$ \text{Latency}_{\text{effective}} = (\text{HitRate}_{\text{PTC}} \times 1) + ((1 - \text{HitRate}_{\text{PTC}}) \times \text{Latency}_{\text{decoder}}) $$
  $$ \text{Latency}_{\text{effective}} = (0.75 \times 1) + (0.25 \times 4) = 0.75 + 1.0 = 1.75 \text{ cycles} $$

### 10.1.3 Expanding the Fusion Window
By bypassing the sequential decoder pipeline on PTC hits, Bemi v3.0 expands its macro-op fusion engine to support **8-pair group-fused execution patterns** (raising the IPC multiplier to **1.60**). 
The effective peak IPC per thread scales significantly:
$$ \text{IPC}_{\text{peak}} = \frac{\text{Width}_{\text{dispatch}}}{\text{Latency}_{\text{effective}}} \times \text{Multiplier}_{\text{fusion}} $$
$$ \text{IPC}_{\text{peak}} = \frac{4}{1.75} \times 1.60 = 3.66 $$
This represents a $2.44\times$ increase over Bemi v2.0's peak IPC ($1.5$).
