# Chapter 11: Bemi v4.0: Ultra-Bandwidth & Execution Zenith Architecture

## 11.1 Adaptive Hardware Memory Compression (Adaptive HMC)

### 11.1.1 The Limitation of Fixed-Ratio Compression
In Bemi v3.0, the hardware link compression engine (HMC) utilized a fixed Base-Delta-Immediate (BDI) algorithm. While BDI has low latency and is easy to implement in silicon, it suffers from structural rigidity. It assumes a homogeneous distribution of data deltas. When confronted with diverse data formatsâ€”such as highly unstructured tensor weights in deep learning or variable-length character blocks in database recordsâ€”BDI's compression ratio degrades, dropping back toward 1.0x and triggering the Bandwidth Governor.

To overcome this, Bemi v4.0 introduces **Adaptive Hardware Memory Compression (Adaptive HMC)**.

```
                         Memory Controller Bus
                                   |
                  +----------------------------------+
                  |    Adaptive Compression Matcher  |
                  +----------------------------------+
                     /             |              \
      (Tensor Data) /      (Database Data) \       \ (Stream Data)
                   v               v                v
            +------------+  +------------+  +------------+
            |  FPC Unit  |  |  FDC Unit  |  |  BDI Unit  |
            | (2.2x Ratio|  | (2.0x Ratio|  | (1.8x Ratio|
            +------------+  +------------+  +------------+
                   \               |              /
                    \              |             /
                     v             v            v
                  +----------------------------------+
                  |     Physical Link Serializer     |
                  +----------------------------------+
```

### 11.1.2 Dynamic Pattern Matching (FPC & FDC)
The Adaptive HMC unit integrates multiple compression engines on the memory controller die, dynamically selecting the optimal compressor based on the active memory block pattern:
- **Frequent Pattern Compression (FPC):** Targets numerical tensor structures and floating-point weights by compressing common bit prefixes. Achieves up to a **2.2x compression ratio** for Deep Learning Training, expanding effective bandwidth to **140.8 GB/s**.
- **Frequent Dictionary Compression (FDC):** Utilizes a small, dynamically updated dictionary array for tabular database columns. Achieves up to a **2.0x compression ratio** for OLAP Scan, expanding effective bandwidth to **128.0 GB/s**.
- **Base-Delta-Immediate (BDI):** Acts as the default fallback for linear memory streams, achieving a **1.8x compression ratio** for Video playback and encoding (**115.2 GB/s**).
