# Chapter 6: Thread Scheduling and Cache Mitigation

## 6.1 Algorithmic Thread Mapping (SMT Maximization)

### 6.1.1 The Illusion of Concurrency
In Section 1.4, we established the fundamental paradox of the Bemi Thread Density model: how presenting 144 logical processors to the guest Operating System yields higher performance on a physical 12-core/24-thread CPU. This chapter breaks down the exact algorithmic mechanisms the Ring -1 firmware uses to execute this micro-architectural scheduling.

A standard x86 Operating System (Ring 0) manages threads using a time-sliced, preemptive scheduler. However, the OS scheduler operates at a macroscopic scale (milliseconds). If an OS thread triggers a cache miss that takes 250 nanoseconds (roughly 1,000 clock cycles) to resolve from physical DRAM, the OS scheduler will not preempt it. The physical execution units on that core simply stall and sit completely idle for 1,000 cycles.

If the physical core utilizes Simultaneous Multithreading (SMT, or Intel Hyper-Threading), it has two hardware threads. While Thread 0 is stalled on memory, Thread 1 can execute. However, if *both* hardware threads stall on memory simultaneously, the entire physical core goes dark.

### 6.1.2 Decoupling Logical and Physical Threads
The Bemi firmware solves this by decoupling the logical OS thread from the physical hardware thread.

When the Bemi BIOS intercepts the boot sequence, it modifies the ACPI (Advanced Configuration and Power Interface) Multiple APIC Description Table (MADT). The OS reads this table and believes there are 144 physical APICs (cores) available. The OS subsequently spawns 144 execution threads to maximize its perceived hardware.

In Ring -1, the Bemi firmware maintains a highly optimized **Scheduler Pool**. 
- **Pool Size:** 144 Virtual Thread Contexts (VTC).
- **Hardware Slots:** 24 Physical Execution Slots.

### 6.1.3 The Bemi Scheduling Algorithm
The Bemi scheduler does not use macroscopic time-slicing. It uses **Event-Driven Micro-Architectural Scheduling**. 

The firmware triggers a scheduling evaluation (a micro-context switch) under two primary conditions:
1. **Timer Preemption:** The VMX Preemption Timer expires (Section 4.3).
2. **JIT Stall Prediction:** The firmware's DBT engine mathematically predicts a pipeline stall.

When the JIT engine translates a block of code (Section 2.1), it flags instructions that are highly likely to cause cache misses (e.g., pointer chasing loops or traversing massive linked lists). The firmware injects a deliberate `VMCALL` instruction (a hypercall) into the emitted binary *immediately before* the predicted stall.

When the physical CPU hits this `VMCALL`, it traps back to Ring -1 *before* the 1,000-cycle memory stall occurs. The firmware issues an asynchronous hardware prefetch for the memory address, instantly saves the thread's architectural state, and swaps in a different logical thread from the pool of 144 that is computationally ready.

**Algorithm 6.1.1: Micro-Architectural Context Switch**
```rust
// Rust-based representation of the Firmware SMT Scheduler
pub fn handle_predicted_stall(vmcs: &mut Vmcs, scheduler: &mut ThreadPool) {
    let current_logical_id = vmcs.read(GUEST_CR3); // Simplified thread ID
    
    // 1. Extract the memory address that is about to cause the stall
    let predicted_miss_address = extract_stall_target(vmcs);
    
    // 2. Issue an asynchronous hardware PREFETCH instruction from Ring -1
    // This forces the physical memory controller to begin fetching the data
    // from DRAM into the L3 cache while the firmware does other work.
    execute_hardware_prefetch(predicted_miss_address);
    
    // 3. Suspend the current thread
    scheduler.save_state(current_logical_id, vmcs);
    scheduler.set_thread_status(current_logical_id, ThreadStatus::WaitingOnMemory);
    
    // 4. Select a computational-heavy thread (e.g., one executing AVX-512 math)
    // to keep the physical ALUs saturated while the memory fetch completes.
    let next_thread_id = scheduler.get_highest_priority_ready_thread();
    
    // 5. Context Switch
    scheduler.restore_state(next_thread_id, vmcs);
    execute_vmlaunch();
}
```

Through Algorithm 6.1.1, the physical execution pipelines are never allowed to stall. The 24 physical SMT threads are constantly fed instructions from the 144-thread pool, achieving utilization rates approaching the theoretical mathematical limit of the silicon.
## 6.2 Cache Contention Mitigation Strategies

### 6.2.1 The Danger of Oversubscription
While the thread over-subscription model detailed in Section 6.1 guarantees that the physical execution units (ALUs) remain saturated, it introduces a massive architectural hazard: **Cache Thrashing**.

A modern physical CPU with 12 cores might possess 32MB of shared L3 Cache. If the physical CPU runs 24 native threads, each thread effectively has $32\text{MB} / 24 \approx 1.33\text{MB}$ of L3 cache at its disposal.

By forcefully multiplexing 144 virtual threads onto that same physical CPU, the Bemi firmware reduces the effective cache footprint per thread to $32\text{MB} / 144 \approx 222\text{KB}$.

If the Working Set Size (WSS) of the guest Operating System applications exceeds 222KB per thread, the threads will begin evicting each other's data from the L3 cache. When Thread A is swapped out to run Thread B, Thread B will overwrite Thread A's data. When Thread A resumes, it will suffer a catastrophic L3 miss, fetching from slow physical DRAM. 

If this is not mitigated mathematically, the Cache Miss Rate ($CMR$) approaches $1.0$ (100%), and the system performance collapses, regardless of how fast the ALUs can process data.

### 6.2.2 Mathematical Cache Modeling in Ring -1
To prevent cache collapse, the Bemi firmware must act as a memory hypervisor. It tracks the memory access patterns of every logical thread.

Because the Bemi firmware intercepts all x86 memory loads and stores during the Dynamic Binary Translation phase (Section 3.3), it builds a spatial profile of each thread. It records which physical memory pages (4KB blocks) a thread frequently accesses.

The firmware scheduler uses a **Cache Locality Graph**. 
- Nodes represent the 144 logical threads.
- Edges between nodes indicate that two threads are accessing the exact same physical memory pages (Shared Memory).
- The weight of the edge represents the frequency of shared access.

### 6.2.3 Translation Cache (TC) Spatial Locality Scheduling
When the firmware scheduler (Algorithm 6.1.1) must select the *next* thread to execute from the pool, it does not pick a thread at random. It queries the Cache Locality Graph.

If Physical Core 0 just finished executing a quantum of Thread A, the L1 and L2 caches of Core 0 are filled with Thread A's data. The scheduler looks for a Thread B in the graph that has a heavily weighted edge connected to Thread A.

If Thread B operates on the exact same data arrays as Thread A, the firmware maps Thread B onto Physical Core 0. When Thread B executes, it experiences massive L1 and L2 Cache Hits, because Thread A already pulled the data into the cache.

**Algorithm 6.2.1: Locality-Aware Thread Scheduling**
```rust
// Rust-based representation of Cache Locality Scheduling
pub struct ThreadMemoryProfile {
    pub logical_id: u32,
    pub active_pages: HashSet<PhysicalPageAddress>,
}

pub fn select_optimal_next_thread(
    physical_core_id: u32, 
    last_executed_thread: u32, 
    pool: &ThreadPool
) -> u32 {
    let last_profile = pool.get_memory_profile(last_executed_thread);
    
    let mut best_thread = 0;
    let mut max_overlap = 0;

    for candidate_thread in pool.get_ready_threads() {
        let candidate_profile = pool.get_memory_profile(candidate_thread);
        
        // Calculate the Set Intersection of physical memory pages
        let overlap = last_profile.active_pages
                                  .intersection(&candidate_profile.active_pages)
                                  .count();
                                  
        if overlap > max_overlap {
            max_overlap = overlap;
            best_thread = candidate_thread;
        }
    }
    
    if max_overlap == 0 {
        // Fallback: Pick a thread with a small Working Set Size
        return pool.get_smallest_wss_thread();
    }

    best_thread
}
```

### 6.2.4 Time-Slicing the Cache (Gang Scheduling)
If the firmware detects that a group of threads share no memory and have massive independent Working Set Sizes that exceed the physical L3 capacity, it employs **Gang Scheduling**.

Instead of rapidly multiplexing all 144 threads and causing L3 thrashing, the firmware partitions the 144 threads into "Gangs" of 24. 
- Gang 1 runs exclusively on the 24 physical threads for a long macroscopic time slice (e.g., 5 milliseconds). Gang 1 takes complete ownership of the 32MB L3 cache.
- The firmware then flushes the pipeline and swaps in Gang 2, giving them total ownership of the cache.

By dynamically analyzing the mathematical intersection of memory accesses in Ring -1, the Bemi BIOS ensures that the massive thread density model actually translates into real-world throughput, expertly riding the razor's edge between ALU saturation and SRAM cache collapse.
