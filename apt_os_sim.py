# apt_os_sim.py
"""
Apt OS Simulator
================
Simulates a modern microkernel-style operating system (Apt OS) running on the Pentium CPU.
Generates instruction streams representing different OS activities:
  1. Boot & Initialization (page table setup, CR3 configuration, driver mapping)
  2. Process Scheduling & Context Switching (forking, running, and context-switching)
  3. Paged Memory Swapping (accessing memory, triggering page walks & faults)
  4. Shell Bytecode Interpreter (strictly serial workload simulating bytecode loop)
  5. Storage Block I/O (handling syscalls, disk interrupts, and block memory copies)
"""

import random

class AptOS:
    def __init__(self):
        self.name = "Apt OS"

    def generate_boot_workload(self):
        """
        Workload 1: OS Boot & Initialization
        Sets up page directories, GDT, IDT, and initializes drivers.
        Generates 50,000 instructions:
          - 75% Arithmetic (setting tables, registers)
          - 20% Memory writes (writing page directory entries)
          - 5% Page walks (validating page table mapping)
        """
        stream = []
        random.seed(42)  # For reproducibility
        
        # We simulate configuring memory mappings from address 0x0000 to 0x10000
        for i in range(50000):
            pc = 0x1000 + i * 4
            r = random.random()
            if r < 0.75:
                stream.append({"pc": pc, "type": "arith"})
            elif r < 0.95:
                # Write page directories (strided memory writes)
                addr = 0x2000 + (i % 256) * 64
                stream.append({"pc": pc, "type": "mem_write", "addr": addr})
            else:
                # Trigger a page table directory read
                stream.append({"pc": pc, "type": "page_walk"})
                
        return stream

    def generate_scheduling_workload(self, cpu_is_bemi):
        """
        Workload 2: Process Scheduling & Context Switching
        Simulates 5 processes sharing a single core.
        Total workload: 500,000 instructions.
        Every 5,000 instructions, a context switch happens.
        Total context switches: 100.
        
        Cheating Protection & Isolation:
          On a stock Pentium (1 thread), each context switch costs 150 cycles (register state save/restore).
          On Bemi, the BIOS has allocated 8 temporal hardware thread contexts.
          Since we run 5 concurrent processes, each can map directly to a hardware thread context.
          Temporal hardware scheduling switches contexts in 0 cycles.
        """
        stream = []
        random.seed(42)
        
        num_processes = 5
        quanta = 5000
        total_switches = 100
        
        for switch in range(total_switches):
            # Run a process quantum
            for i in range(quanta):
                pc = 0x5000 + (switch % num_processes) * 0x1000 + i * 4
                stream.append({"pc": pc, "type": "arith"})
                
            # Perform a context switch
            pc_switch = 0x4000 + switch * 4
            if cpu_is_bemi:
                # Bemi's temporal hardware threads absorb the context switch overhead (0 cycles)
                # We model this by not generating a context_switch event (or replacing it with a tiny register select instruction)
                stream.append({"pc": pc_switch, "type": "arith"})
            else:
                # Stock Pentium pays full register save/restore latency
                stream.append({"pc": pc_switch, "type": "context_switch"})
                
        return stream

    def generate_memory_swapping_workload(self):
        """
        Workload 3: Paged Memory Swapping
        Accesses large data arrays. Triggering L1 misses, TLB page walks, and page faults.
        Total workload: 100,000 instructions.
          - 80% Arithmetic
          - 16% Data accesses (reads/writes)
          - 3% Page walks (TLB misses)
          - 1% Page faults (swapping from disk)
        """
        stream = []
        random.seed(42)
        
        for i in range(100000):
            pc = 0x20000 + i * 4
            r = random.random()
            if r < 0.80:
                stream.append({"pc": pc, "type": "arith"})
            elif r < 0.96:
                # Memory read/write with strided address to simulate cache hits/misses
                # Reaches outside 8KB L1 cache to trigger misses
                addr = 0x50000 + (i % 1024) * 64
                mtype = "mem_read" if i % 2 == 0 else "mem_write"
                stream.append({"pc": pc, "type": mtype, "addr": addr})
            elif r < 0.99:
                stream.append({"pc": pc, "type": "page_walk"})
            else:
                # Page fault! Triggers an OS page fault handler (which performs disk reads)
                # We simulate this by appending a sequence of page walk and memory operations
                stream.append({"pc": pc, "type": "page_walk"})
                for j in range(5):
                    stream.append({"pc": pc + j + 1, "type": "mem_read", "addr": 0x90000 + j * 64})
                    
        return stream

    def generate_interpreted_workload(self):
        """
        Workload 4: Shell Bytecode Interpreter (Serial Workload)
        Strictly serial loop (e.g. Python interpreter executing shell commands).
        Amdahl's law bottleneck: cannot run on multiple threads.
        Workload: 100,000 bytecode operations (translated to ~500,000 instructions).
        Each bytecode:
          - Fetch instruction (memory read)
          - Decode op (4 arith instructions)
          - Dispatch (indirect branch, highly mispredicted)
          - Execute (5 arith instructions)
        """
        stream = []
        random.seed(42)
        
        bytecode_count = 100000
        
        for i in range(bytecode_count):
            pc_base = 0x30000 + i * 40
            
            # 1. Fetch
            stream.append({"pc": pc_base, "type": "mem_read", "addr": pc_base})
            
            # 2. Decode (4 instructions)
            for j in range(4):
                stream.append({"pc": pc_base + 4 + j*4, "type": "arith"})
                
            # 3. Dispatch (Indirect branch with 50% chance of being mispredicted)
            # We alternate taken/not-taken to challenge the branch predictors
            taken = (i % 2 == 0)
            stream.append({"pc": pc_base + 20, "type": "branch", "taken": taken, "addr": pc_base + 40})
            
            # 4. Execute (5 instructions)
            for j in range(5):
                stream.append({"pc": pc_base + 24 + j*4, "type": "arith"})
                
        return stream

    def generate_block_io_workload(self):
        """
        Workload 5: Storage Block I/O
        Simulates storage reads/writes.
        Workload: 1,000 disk page I/O requests.
        Each request triggers:
          - A syscall (INT 21h)
          - A hardware disk interrupt
          - A memory copy of 128 bytes (2 cache lines)
        """
        stream = []
        random.seed(42)
        
        num_requests = 1000
        
        for req in range(num_requests):
            pc = 0x40000 + req * 100
            
            # 1. Trigger Syscall
            stream.append({"pc": pc, "type": "syscall"})
            
            # 2. Syscall handler execution (arith + memory write)
            for i in range(20):
                stream.append({"pc": pc + 4 + i*4, "type": "arith"})
            
            # 3. Disk Interrupt
            stream.append({"pc": pc + 84, "type": "syscall"}) # Simulates interrupt vectoring
            
            # 4. Copy 2 cache lines from disk controller buffer to page cache
            stream.append({"pc": pc + 88, "type": "mem_read", "addr": 0xA0000 + req * 128})
            stream.append({"pc": pc + 92, "type": "mem_write", "addr": 0xB0000 + req * 128})
            stream.append({"pc": pc + 96, "type": "mem_read", "addr": 0xA0000 + req * 128 + 64})
            stream.append({"pc": pc + 100, "type": "mem_write", "addr": 0xB0000 + req * 128 + 64})
            
        return stream
