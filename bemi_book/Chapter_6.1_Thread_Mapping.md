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
