# Chapter 11: Bemi v4.0: Ultra-Bandwidth & Execution Zenith Architecture

## 11.3 Neural Perceptron Predictor (NPP)

### 11.3.1 Replacing TAGE with Neural Networks
Branch prediction accuracy is a primary driver of instruction throughput in deep pipelines. When a branch is mispredicted, the processor must flush the execution pipeline, discarding dozens of cycles of speculative work. 

Traditional predictors, such as TAGE, rely on tables of counters indexed by global branch histories. While highly effective, they scale poorly when confronted with complex, interleaved branch patterns from 72 virtual threads.

Bemi v4.0 replaces the TAGE array with a hardware-integrated **Neural Perceptron Predictor (NPP)**.

```
Global History Register (GHR) ---> [ Weight Matrix (SRAM) ]
                                          |
                                          v
                              +-----------------------+
                              | Dot Product Accumulator|
                              +-----------------------+
                                          | (Sign Evaluation)
                                          v
                                   [ Taken / NT ]
```

### 11.3.2 NPP Mechanics and PTC Hit Rates
The NPP models branches as single-layer perceptrons, computing branch predictions using a dot product of global branch history bits against a dynamically trained weight matrix:
$$ y = x_0 w_0 + \sum_{i=1}^{H} x_i w_i $$
- **Hit Rate:** Increases the Ring -1 PTC Trace Cache hit rate to **88%** (up from 75% in v3.0).
- **Effective Decode Latency:**
  $$ \text{Latency}_{\text{effective}} = (0.88 \times 1) + (0.12 \times 4) = 0.88 + 0.48 = 1.35 \text{ cycles} $$
- **10-Pair Fusion:** The ultra-accurate prediction stream enables the decoder to group up to **10-pair macro-op fusion** sequences (raising the fusion multiplier to **1.75**).
- **Peak IPC Throughput:**
  $$ \text{IPC}_{\text{peak}} = \frac{4}{1.35} \times 1.75 = 5.18 $$
