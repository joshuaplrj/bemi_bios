# Chapter 9: Bemi v2.0: Scaled Dominance Architecture

## 9.1 Adaptive Thread Scheduling (84 to 48)

### 9.1.1 The Pitfalls of Thread Oversubscription
In the early design phases of the Bemi BIOS (exemplified by version 1.2 and 1.3), the architecture prioritized high hardware thread density as the primary mechanism for bypassing memory latency. By presenting up to 144 virtual threads (in v1.2) or 84 virtual threads (in v1.3) to the guest Operating System, the scheduler aimed to hide cache-miss latency by rapidly rotating executing threads. The assumption was that the sheer volume of threads would guarantee that at least one thread would always be ready to execute on the physical Arithmetic Logic Units (ALUs).

However, physical implementation under realistic silicon constraints revealed a catastrophic trade-off: **L1 Cache Thinning**. 

Modern processor caches are fabricated with high-density SRAM cells. Because physical die area is strictly limited by semiconductor economics, the total L1 cache budget per core is capped (typically 32 KB for instructions and 32 KB for data). When this budget is sliced across 84 virtual threads, each thread is allocated a meager $4.57 \text{ KB}$ of L1 cache. When sliced across 144 threads, this drops to a microscopic $2.67 \text{ KB}$. 

This cache thinning triggers **thrashing**. As threads are swapped in and out of the execution units, their working sets constantly overwrite each other in the tiny L1 cache. The cache miss rate spikes, converting the intended latency-hiding mechanism into a latency-generation engine.

### 9.1.2 Rebalancing the Thread-to-Cache Ratio
To resolve this thrashing, Bemi v2.0 introduces the **Adaptive Thread count** model. Rather than maximizing thread density at all costs, the architecture optimizes the thread-to-cache ratio. 

Bemi v2.0 reduces the virtual thread count per core from 7 (in v1.3) to 4 (SMT-4), resulting in a chip-level total of **48 virtual threads** across the 12 physical cores. 

By reclaiming 42% of the thread context overhead, the Bemi v2.0 silicon allocation increases the private L1 Cache capacity per thread to a healthy **8.0 KB** (a 75% increase over v1.3). This expanded cache footprint allows typical working sets (such as loop variables and local pointer targets) to remain resident in cache across thread-scheduling epochs, immediately stabilizing instruction throughput.
