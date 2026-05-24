# Chapter 2: Dynamic Binary Translation (DBT) on Native x86

## 2.1 The Translation Engine Algorithms

### 2.1.1 The Mechanics of Instruction Interception
The foundational requirement of the Bemi BIOS is the ability to intercept execution before the physical CPU evaluates native, unoptimized x86 instructions. Because the Bemi BIOS operates as a hypervisor in Ring -1 (Root Mode), it configures the physical processor's Extended Page Tables (EPT) to trigger a `VMExit` exception whenever the guest Operating System attempts to execute code from an unknown or un-translated memory page.

When the OS scheduler dispatches a thread to an un-translated page, the physical CPU traps into the Bemi firmware. At this precise microsecond, the physical CPU's stateâ€”including its Instruction Pointer (`RIP`), general-purpose registers, and CPU flagsâ€”is saved to the Virtual Machine Control Structure (VMCS). The firmware's Dynamic Binary Translation (DBT) engine now possesses the exact memory address of the unoptimized x86 code block that the OS wishes to execute.

### 2.1.2 The Basic Block Boundary Constraints
The DBT engine does not translate entire programs at once. That would be computationally prohibitive and mathematically undecidable (due to the Halting Problem). Instead, the translation unit is strictly constrained to a **Basic Block**.

**Definition 2.1.1: Basic Block (BB)**
A Basic Block is a sequence of contiguous instructions with exactly one entry point and one exit point. The exit
<truncated 21935 bytes>

    memory.write_bytes(jump_address, &patch_bytes);
    
    // 4. Update the firmware state map
    block_a.linked_to = Some(block_b.original_x86_rip);
}
```
By aggressively linking blocks together, the physical CPU can execute millions of optimized instructions entirely within the Translation Cache, running at full native clock speed without ever triggering a Ring -1 `VMExit`. 

### 2.4.4 Cache Invalidation and Self-Modifying Code
The most complex edge case in managing the Translation Cache is maintaining coherency with the guest OS's memory space. 

Modern Operating Systems and applications occasionally utilize Self-Modifying Code (SMC) or JIT compilers of their own (e.g., JavaScript engines like V8). If the guest OS overwrites a block of x86 instructions in Ring 0 memory, but the Bemi firmware continues to execute the old, translated version from the Ring -1 TC, the system will catastrophically crash.

To enforce absolute coherency, the Bemi firmware uses the Extended Page Tables (EPT) to write-protect any physical memory page that contains x86 code that has been translated into the TC. 

If the guest OS attempts to write data to one of these protected pages, the physical CPU triggers an `EPT Violation` trap into the Ring -1 firmware. The firmware instantly:
1. Identifies which Basic Blocks in the Translation Cache correspond to the modified page.
2. Mathematically un-links any connections to those blocks (Algorithm 2.4.1).
3. Invalidates and flushes the modified blocks from the TC.
4. Removes the write-protection, allows the OS to complete the memory write, and seamlessly resumes execution.

This rigorous, hardware-enforced invalidation protocol guarantees that the highly optimized Translation Cache remains a perfect, deterministic reflection of the guest Operating System's intended logic.
