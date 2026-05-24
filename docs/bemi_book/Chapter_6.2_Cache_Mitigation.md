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
