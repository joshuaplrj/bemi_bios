# Chapter 9: Bemi v2.0: Scaled Dominance Architecture

## 9.6 The Bandwidth Governor

### 9.6.1 The "Race to Stall" Memory Bottleneck
When 48 threads execute concurrently, memory-bound workloads (such as Deep Learning Training and OLAP Scan) generate a massive volume of memory read/write requests. If the total bandwidth requested by the active threads exceeds the physical dual-channel DDR5 bus limit ($64 \text{ GB/s}$), the memory controller becomes saturated.

Once the memory bus saturates, queue latency in the memory controller spikes exponentially. Threads stall indefinitely, and the dynamic power of the memory interface rises to its limit, generating intense heat. This state is known as **DRAM Saturation Thrashing**.

### 9.6.2 The Bandwidth Governor Control Loop
To prevent this, Bemi v2.0 implements a hardware **Bandwidth Governor`** in the memory controller:

1. **Measurement:** A hardware counter monitors memory transactions within a sliding $1000\text{-cycle}$ execution window.
2. **Detection:** If the measured bandwidth exceeds $85\%$ of the physical limit ($54.4 \text{ GB/s}$), the Governor triggers a throttle event.
3. **Mitigation:** The thread scheduler is commanded to de-schedule $25\%$ of the active threads (focusing on low-priority or memory-bound background tasks), forcing them into a wait state.
4. **Recovery:** By reducing active request streams, the memory controller queue drains, dropping utilization. When bandwidth falls below $60\%$ ($38.4 \text{ GB/s}$), the Governor releases the throttle, restoring full thread execution.

This control loop prevents memory bus saturation, ensuring that the system never enters the thrashing state and maintaining optimal performance-per-watt.
