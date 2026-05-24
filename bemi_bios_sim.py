# bemi_bios_sim.py
"""
Bemi BIOS and Legacy BIOS Simulator
===================================
Defines the bios configurations.
Bemi BIOS v7.2 "Zero-Footprint Singularity" runs a dynamic resource allocation algorithm:
  - Queries CPU cache size and physical characteristics.
  - Dynamically calculates the optimal temporal threads, ROB size, L0 cache allocation,
    memory compression ratio, and branch prediction table sizes based on available SRAM.
  - Applies these parameters to the CPU virtualization profile (Ring -1 DBT).
"""

import math

class BIOS:
    def __init__(self, name):
        self.name = name

    def boot(self, cpu):
        raise NotImplementedError()


class LegacyBIOS(BIOS):
    def __init__(self):
        super().__init__("Legacy BIOS (Standard PC)")

    def boot(self, cpu):
        print(f"\n  [{self.name}] Booting stock hardware...")
        cpu.apply_virtualization_profile(None)
        print("  Legacy BIOS boot complete. Running on native CISC Pentium CPU.")


class BemiBIOS(BIOS):
    def __init__(self):
        super().__init__("Bemi BIOS v7.2 (Zero-Footprint Singularity)")

    def boot(self, cpu):
        print(f"\n  [{self.name}] Booting...")
        print("  [Bemi] Querying host CPU capabilities via Ring -1 firmware registers...")
        
        # 1. Query Hardware Specs
        # In a real environment, BIOS reads CPUID and MSRs. Here we query the cpu instance:
        cpu_cores = 1
        i_cache_kb = cpu.i_cache.size_bytes // 1024
        d_cache_kb = cpu.d_cache.size_bytes // 1024
        total_l1_cache_kb = i_cache_kb + d_cache_kb
        mem_latency_cycles = cpu.base_mem_latency
        
        print(f"  [Bemi] Detected: {cpu_cores} physical core(s) | {total_l1_cache_kb} KB L1 cache | {mem_latency_cycles}-cycle EDO DRAM")
        
        # 2. Run Resource Allocation Optimization Algorithm
        # Bemi must decide:
        #   - virtual_threads (T)
        #   - L0 trace/data cache allocation (L0_kb)
        #   - ROB entries (R)
        #   - Memory compression ratio (C_ratio)
        #   - MLP depth (MLP)
        #
        # Constraints:
        #   - Total SRAM available = total_l1_cache_kb * 1024 bytes (16384 bytes).
        #   - Thread State Memory = T * 64 bytes (context registers, flags, page table cache).
        #   - ROB Entry Memory = R * 2 bytes (2B compressed instruction entries in split/distributed layout).
        #   - L0 Cache Memory = L0_kb * 1024 bytes.
        #   - Branch prediction (NPP) table + buffers = 1024 bytes.
        #   - Memory compression metadata buffers = 512 bytes.
        #   - Formula: (T * 64) + (R * 2) + (L0_kb * 1024) + 1024 + 512 <= SRAM_bytes
        
        sram_bytes = total_l1_cache_kb * 1024
        reserved_bytes = 1024 + 512  # NPP + memory compression buffers
        usable_sram = sram_bytes - reserved_bytes  # 16384 - 1536 = 14848 bytes
        
        # We want to optimize throughput. Throughput is proportional to T (threads) and ROB depth R,
        # but if we allocate too many threads, our L0 cache size must shrink, which increases miss penalty.
        # Let's search the configuration space to find the optimal combination.
        # We sweep T from 2 to 16.
        # For each T:
        #   - Set ROB R = T * 32 (each thread gets a 32-entry OoO window).
        #   - Calculate remaining bytes for L0 cache: remaining = usable_sram - (T * 64) - (R * 2).
        #   - Set L0_kb = floor(remaining / 1024).
        #   - Throughput metric = T * (1.0 - exp(-R / 64)) * (L0_kb / 8.0)
        
        best_t = 8
        best_rob = 256
        best_l0_kb = 8
        best_score = 0
        
        for t in range(2, 17, 2):
            rob = t * 32
            thread_state_cost = t * 64
            rob_cost = rob * 2
            remaining_bytes = usable_sram - thread_state_cost - rob_cost
            l0_kb = remaining_bytes // 1024
            
            if l0_kb < 1:
                continue
                
            # Score modeling: trades thread count against L0 cache size (hiding DRAM stalls)
            # score = T * ROB_benefit * Cache_benefit
            rob_benefit = 1.0 - math.exp(-rob / 128.0)
            cache_benefit = l0_kb / 8.0  # Normalized to an 8KB L1 cache size
            score = t * rob_benefit * cache_benefit
            
            if score > best_score:
                best_score = score
                best_t = t
                best_rob = rob
                best_l0_kb = l0_kb

        # 3. Derive Architectural Parameters from Optimization
        #   - Memory Compression Ratio: Higher compression is possible if L0 cache is large
        mem_compression = 1.5 + (best_l0_kb / 16.0) # e.g. 1.5 + 8/16 = 2.0x
        
        #   - MLP: outstanding misses = ROB / 16
        mlp = max(4.0, min(16.0, best_rob / 16.0)) # e.g. 256 / 16 = 16.0
        
        #   - NPP Hit Rate: based on branch table size (repurposed from L1)
        branch_hit_rate_npp = 0.70 + (best_l0_kb / 64.0) # e.g. 0.70 + 8/64 = 0.825 (82.5%)
        
        #   - L0 hit rate: 85% if L0 is 8KB, scales with size
        l0_hit_rate = 0.50 + (best_l0_kb / 24.0) # e.g. 0.50 + 8/24 = 0.833 (83.3%)
        if l0_hit_rate > 0.95: l0_hit_rate = 0.95
        
        #   - Effective Decode Latency: Trace cache bypass
        decode_latency = 4.0 * (1.0 - branch_hit_rate_npp) # e.g. 4.0 * (1 - 0.8) = 0.8 cycles
        if decode_latency < 0.5: decode_latency = 0.5
        
        #   - Fusion Bonus: Super-op fusion
        fusion_bonus = 1.2 + (best_rob / 1024.0) # e.g. 1.2 + 256/1024 = 1.45x
        
        #   - TDP reduction: simpler RISC cores run cooler
        tdp_watts = 10.0 * 0.85 # 8.5W
        
        # Interrupt latencies are lower because they run as pre-translated trace-cache hits
        syscall_cost = 8
        hw_int_cost = 20
        branch_penalty = 4
        
        profile = {
            "threads": best_t,
            "decode_latency": decode_latency,
            "l0_hit_rate": l0_hit_rate,
            "mem_compression": mem_compression,
            "mlp": mlp,
            "fusion_bonus": fusion_bonus,
            "branch_penalty": branch_penalty,
            "branch_hit_rate_npp": branch_hit_rate_npp,
            "syscall_cost": syscall_cost,
            "hw_int_cost": hw_int_cost,
            "tdp_watts": tdp_watts
        }
        
        print("\n  [Bemi Dynamic Resource Optimization Decisions]")
        print(f"    SRAM Budget Allocated : {sram_bytes} bytes (100% of L1)")
        print(f"    SRAM Partitioning     : Thread States = {best_t * 64}B | ROB = {best_rob * 2}B | L0 Cache = {best_l0_kb}KB | Buffers = {reserved_bytes}B")
        print(f"    Temporal Threads (T)  : {best_t} (Dynamic Temporal SMT)")
        print(f"    ROB Depth (R)         : {best_rob} entries (Compressed 2B split/banked ROB)")
        print(f"    L0 Cache Size         : {best_l0_kb} KB (Absorbs {l0_hit_rate*100:.1f}% memory accesses)")
        print(f"    Memory Compression    : {mem_compression:.2f}x (Boosts effective bandwidth)")
        print(f"    Effective Memory Lat  : {mem_latency_cycles / mlp:.2f} cycles (MLP-{int(mlp)} overlap)")
        print(f"    Branch Predictor (NPP): {branch_hit_rate_npp*100:.1f}% hit rate | {branch_penalty}c penalty")
        print(f"    Effective Decode Lat  : {decode_latency:.2f} cycles (via Trace Cache hits)")
        print(f"    Super-Op Fusion Bonus : {fusion_bonus:.2f}x")
        print(f"    Dynamic Power (TDP)   : {tdp_watts:.1f} Watts")
        
        # Apply the dynamically decided profile to the CPU virtualization layer
        cpu.apply_virtualization_profile(profile)
        print("  Bemi BIOS boot complete. Ring -1 DBT Hypervisor successfully armed.")
