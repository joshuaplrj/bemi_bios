# Chapter 9: Bemi v2.0: Scaled Dominance Architecture

## 9.3 Independent ROB Bank Partitioning

### 9.3.1 The $O(N^2)$ Reorder Buffer Penalty
In out-of-order (OoO) processors, the Reorder Buffer (ROB) tracks instructions that have been dispatched but not yet retired, allowing the CPU to execute instructions out of their original program order while maintaining precise exception state at retire. 

To determine when an instruction's source operands are ready, the ROB utilizes Content-Addressable Memory (CAM). Every time an instruction finishes execution and writes back its result, the ROB must broadcast the destination register tag to all pending entries. The power consumption and silicon area of this CAM broadcast logic scales quadratically with the number of ROB entries:
$$ \text{Cost}_{\text{ROB}} \propto O(N^2) \quad \text{where } N \text{ is the number of ROB entries} $$

In Bemi v1.3, the architecture shared a single 784-entry ROB across all SMT threads on a core. The CAM broadcast logic required to support associative lookup across 784 entries would violate physical silicon area limits and exceed the thermal envelope.

### 9.3.2 Slicing the ROB into Independent Banks
Bemi v2.0 resolves the $O(N^2)$ penalty by partitioning the core's ROB budget into **4 independent ROB banks** per core. 

- **Allocation:** Each of the 4 SMT threads is mapped to a dedicated, physically isolated **196-entry ROB bank**.
- **No Shared CAM:** Because each bank is dedicated to a single thread, tag broadcast is strictly confined to that bank's 196 entries. The associative lookup cost drops significantly:
  $$ \text{Cost}_{\text{v2.0}} \propto 4 \times O(196^2) \ll O(784^2) $$
- **Isolation:** There is zero thread interference in the ROB. If one thread stalls on a cache miss, its private 196-entry ROB bank may fill up, but the other three banks remain fully active, executing instructions from the other threads.
