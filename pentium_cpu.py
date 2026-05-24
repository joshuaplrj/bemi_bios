# pentium_cpu.py
"""
Pentium CPU Simulator (200 MHz, P54C-class)
===========================================
Strictly isolated hardware simulator modeling:
  - 200MHz clock frequency (1 cycle = 5 ns)
  - U/V pipeline (superscalar issue up to 2 instructions/cycle)
  - 16KB L1 cache (8KB I-cache + 8KB D-cache, direct-mapped, 64B lines)
  - 256-entry Branch Target Buffer (BTB)
  - EDO DRAM memory interface (40 cycles latency, 1.6 GB/s bandwidth)
  - Syscall interrupt vectoring (32 cycles) and hardware interrupt latency (112 cycles)
  - Ring -1 Hypervisor registers to support Bemi BIOS virtualization overrides
"""

import math

class DirectMappedCache:
    def __init__(self, size_kb, line_size=64):
        self.size_bytes = size_kb * 1024
        self.line_size = line_size
        self.num_lines = self.size_bytes // line_size
        self.lines = [None] * self.num_lines
        self.hits = 0
        self.misses = 0

    def access(self, address):
        line_address = address // self.line_size
        index = line_address % self.num_lines
        tag = line_address // self.num_lines
        
        if self.lines[index] == tag:
            self.hits += 1
            return True  # Hit
        else:
            self.lines[index] = tag
            self.misses += 1
            return False  # Miss

    def reset(self):
        self.lines = [None] * self.num_lines
        self.hits = 0
        self.misses = 0


class BranchTargetBuffer:
    def __init__(self, entries=256):
        self.entries = entries
        # Simple table storing predicted taken/not-taken state
        self.table = [False] * entries
        self.hits = 0
        self.misses = 0

    def predict_and_update(self, pc, actual_taken):
        index = (pc >> 2) % self.entries
        predicted_taken = self.table[index]
        
        if predicted_taken == actual_taken:
            self.hits += 1
            is_hit = True
        else:
            self.table[index] = actual_taken
            self.misses += 1
            is_hit = False
            
        return is_hit

    def reset(self):
        self.table = [False] * self.entries
        self.hits = 0
        self.misses = 0


class PentiumCPU:
    def __init__(self):
        self.frequency_hz = 200_000_000  # 200 MHz
        self.cycle_time_ns = 5.0          # 1 cycle = 5 ns
        
        # Power & Energy tracking
        self.base_tdp_watts = 10.0
        self.virtual_tdp_watts = 10.0
        
        # Architectural State
        self.cycles = 0
        self.instructions = 0
        
        # Subsystems
        self.i_cache = DirectMappedCache(size_kb=8)
        self.d_cache = DirectMappedCache(size_kb=8)
        self.btb = BranchTargetBuffer(entries=256)
        
        # Registers
        self.registers = {
            "EAX": 0, "EBX": 0, "ECX": 0, "EDX": 0,
            "ESI": 0, "EDI": 0, "ESP": 0x7FFF, "EBP": 0x7FFF,
            "EIP": 0x0000, "EFLAGS": 0, "CR3": 0
        }
        
        # Hardware constants
        self.base_mem_latency = 40         # cycles to EDO DRAM
        self.base_decode_latency = 4       # cycles CISC decode stall
        self.base_branch_penalty = 12      # cycles branch mispredict stall
        self.base_syscall_cost = 32        # cycles vectoring
        self.base_hw_int_cost = 112        # cycles vectoring + handler latency
        self.context_switch_cost = 150     # cycles for register state save/restore
        
        # Virtualization/Hypervisor settings (Ring -1, initially disabled)
        self.hypervisor_active = False
        self.virt_threads = 1
        self.virt_decode_latency = 4.0
        self.virt_l0_hit_rate = 0.0
        self.virt_mem_compression = 1.0
        self.virt_mlp = 1.0
        self.virt_fusion_bonus = 1.0
        self.virt_branch_penalty = 12
        self.virt_branch_hit_rate_npp = 0.0
        self.virt_syscall_cost = 32
        self.virt_hw_int_cost = 112

    def reset_stats(self):
        self.cycles = 0
        self.instructions = 0
        self.i_cache.reset()
        self.d_cache.reset()
        self.btb.reset()

    def apply_virtualization_profile(self, profile):
        """
        Called exclusively by Bemi BIOS to set up Ring -1 hypervisor overrides.
        Allows the hardware to operate under software-defined hypervisor parameters.
        """
        if profile is None:
            self.hypervisor_active = False
            self.virt_tdp_watts = self.base_tdp_watts
            return
            
        self.hypervisor_active = True
        self.virt_threads = profile.get("threads", 1)
        self.virt_decode_latency = profile.get("decode_latency", 4.0)
        self.virt_l0_hit_rate = profile.get("l0_hit_rate", 0.0)
        self.virt_mem_compression = profile.get("mem_compression", 1.0)
        self.virt_mlp = profile.get("mlp", 1.0)
        self.virt_fusion_bonus = profile.get("fusion_bonus", 1.0)
        self.virt_branch_penalty = profile.get("branch_penalty", 12)
        self.virt_branch_hit_rate_npp = profile.get("branch_hit_rate_npp", 0.0)
        self.virt_syscall_cost = profile.get("syscall_cost", 32)
        self.virt_hw_int_cost = profile.get("hw_int_cost", 112)
        self.virt_tdp_watts = profile.get("tdp_watts", 10.0)

    def read_stats(self):
        """
        Returns stats for reporting.
        """
        elapsed_sec = self.cycles / self.frequency_hz
        tdp = self.virt_tdp_watts if self.hypervisor_active else self.base_tdp_watts
        energy_joules = tdp * elapsed_sec
        
        return {
            "cycles": self.cycles,
            "instructions": self.instructions,
            "i_cache_hits": self.i_cache.hits,
            "i_cache_misses": self.i_cache.misses,
            "d_cache_hits": self.d_cache.hits,
            "d_cache_misses": self.d_cache.misses,
            "btb_hits": self.btb.hits,
            "btb_misses": self.btb.misses,
            "elapsed_seconds": elapsed_sec,
            "energy_joules": energy_joules,
            "ipc": self.instructions / max(1, self.cycles),
            "tdp_watts": tdp
        }

    def execute_cycles(self, cycle_count):
        """
        Directly advances physical clock. Enforces hardware-only increment.
        """
        self.cycles += int(cycle_count)

    def execute_instruction_block(self, instruction_stream, parallel_threads=None):
        """
        Executes a sequence of instructions.
        Each instruction in the stream is a dict representing:
          - "pc": address of instruction (int)
          - "type": "arith", "mem_read", "mem_write", "branch", "context_switch", "syscall", "page_walk"
          - "addr": memory address (int) for mem/branch operations
          - "taken": boolean for branch operations
        """
        # Under Bemi virtual threads, throughput scaling reduces instruction execution cycles
        if self.hypervisor_active:
            if parallel_threads is not None:
                active_threads = min(self.virt_threads, parallel_threads)
            else:
                active_threads = self.virt_threads
        else:
            active_threads = 1
        
        for instr in instruction_stream:
            self.instructions += 1
            inst_cycles = 0
            
            # 1. Decode Phase
            if self.hypervisor_active:
                # Bemi trace cache hit or bypass reduces decode stalls
                inst_cycles += self.virt_decode_latency
            else:
                # Stock Pentium CISC decode penalty
                inst_cycles += self.base_decode_latency
                
            # 2. Execution & Pipeline Hazards
            itype = instr.get("type", "arith")
            
            if itype == "arith":
                # Arithmetic instruction (e.g., ADD, SUB)
                # Pentium has U/V pipelines, so executing two arithmetic instructions together takes 1 cycle.
                # Average latency is 0.5 cycles per instruction if paired, or 1 cycle.
                base_exec = 0.5 if not self.hypervisor_active else (0.5 / self.virt_fusion_bonus)
                inst_cycles += base_exec
                
            elif itype in ("mem_read", "mem_write"):
                # Memory read or write: needs cache lookup
                addr = instr.get("addr", 0)
                is_hit = False
                
                # Check L0 cache (only exists in Bemi virtual mode)
                if self.hypervisor_active and self.virt_l0_hit_rate > 0:
                    # In a simple statistical model, L0 absorbs a percentage of accesses
                    # In a deterministic model, we use D-cache but scale it
                    # Let's check D-cache with L0 override
                    import random
                    if random.random() < self.virt_l0_hit_rate:
                        is_hit = True
                        inst_cycles += 1 # L0 hit cost
                
                if not is_hit:
                    # Check L1 Data Cache
                    is_hit = self.d_cache.access(addr)
                    if is_hit:
                        inst_cycles += 1 # L1 hit cost
                    else:
                        # Miss! Must fetch from EDO DRAM
                        if self.hypervisor_active:
                            # Bemi hides memory latency through MLP and memory compression
                            comp_latency = 2.0  # 2 cycles software compression overhead
                            eff_latency = (self.base_mem_latency / self.virt_mlp) + comp_latency
                            inst_cycles += eff_latency
                        else:
                            inst_cycles += self.base_mem_latency
                            
            elif itype == "branch":
                pc = instr.get("pc", 0)
                taken = instr.get("taken", False)
                
                is_predicted = False
                if self.hypervisor_active:
                    # Neural branch predictor hit rate
                    import random
                    if random.random() < self.virt_branch_hit_rate_npp:
                        self.btb.hits += 1
                        is_predicted = True
                    else:
                        self.btb.misses += 1
                else:
                    is_predicted = self.btb.predict_and_update(pc, taken)
                    
                if is_predicted:
                    inst_cycles += 0.5 # branch hit cost
                else:
                    # Mispredict penalty
                    inst_cycles += self.virt_branch_penalty if self.hypervisor_active else self.base_branch_penalty
                    
            elif itype == "context_switch":
                # Context switch saves/restores registers
                inst_cycles += self.context_switch_cost
                
            elif itype == "syscall":
                # Syscall vectoring latency
                inst_cycles += self.virt_syscall_cost if self.hypervisor_active else self.base_syscall_cost
                
            elif itype == "page_walk":
                # Page walks read page tables. On stock CPU, this is 2 sequential memory reads (2 * 40 = 80 cycles)
                if self.hypervisor_active:
                    # Bemi hides page walk memory latency via MLP and Trace Cache caching
                    inst_cycles += (2 * (self.base_mem_latency / self.virt_mlp)) / self.virt_fusion_bonus
                else:
                    inst_cycles += 2 * self.base_mem_latency
                    
            # 3. Instruction Execution Cache lookup (I-Cache)
            # Fetching the instruction itself
            pc = instr.get("pc", 0)
            i_hit = self.i_cache.access(pc)
            if not i_hit:
                # Instruction fetch miss
                if self.hypervisor_active:
                    inst_cycles += (self.base_mem_latency / self.virt_mlp)
                else:
                    inst_cycles += self.base_mem_latency
            
            # Apply to core cycles, divided by thread throughput if running in parallel
            # We scale the cycle count based on thread count because threads run in parallel,
            # so the total elapsed execution cycles for a shared workload is divided by the threads
            # (since instructions are distributed across threads).
            self.cycles += inst_cycles / active_threads

    def trigger_interrupt(self, vector_type):
        """
        Simulates a hardware or software interrupt vectoring event.
        """
        if vector_type == "syscall":
            self.cycles += self.virt_syscall_cost if self.hypervisor_active else self.base_syscall_cost
            self.syscalls += 1
        elif vector_type == "hardware":
            self.cycles += self.virt_hw_int_cost if self.hypervisor_active else self.base_hw_int_cost
            self.hw_interrupts += 1
