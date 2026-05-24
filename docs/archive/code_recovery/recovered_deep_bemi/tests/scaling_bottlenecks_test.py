    x86_total = x86_tp * x86_bw_scale

    # --- v2.0 Bemi ---
    v20_rob_per_thread = V20_ROB_PER_THREAD  # 196 (independent bank, NOT partitioned)

    # L0 micro-cache: 70% of accesses hit L0 at 1-cycle, never touch L1
    l0_hit_rate = V20_L0_HIT_RATE  # 0.70
    l1_access_fraction = 1.0 - l0_hit_rate  # 0.30

    # Effective L1 cache per thread (only 30% of accesses reach L1)
    eff_l1_streams = V20_THREADS * l1_access_fraction  # 14.4 effective streams
    eff_l1_per_thread = (L1_PER_CORE_KB * PHYSICAL_CORES) / eff_l1_streams  # ~26.7 KB
    v20_miss = l1_access_fraction * miss_rate_at_cache_kb(eff_l1_per_thread)
    # Combined miss rate: ~2.3%

    # MLP: deep independent ROB enables overlapping 6 cache misses
    v20_mlp = min(V20_MLP_MAX, v20_rob_per_thread / V20_MLP_WINDOW)  # 6.0
    v20_eff_mem_lat = MEM_MISS_LAT / v20_mlp  # 6.67 cycles

    # Average memory access time (L0 filters + L1 + MLP)
    v20_avg_mem = (l0_hit_rate * V20_L0_LATENCY +
                   l1_access_fraction * (1 - v20_miss / l1_access_fraction if l1_access_fraction > 0 else 0) * L1_HIT_LATENCY +
                   v20_miss * v20_eff_mem_lat)

    # IPC with full independent ROB (196 entries, not shared)
    v20_ipc_raw = w["ipc_max"] * (1.0 - math.exp(-v20_rob_per_thread / w["k_sat"]))

    # Stall model
    v20_stall = max(0.0, v20_avg_mem - (v20_rob_per_thread / max(v20_ipc_raw, 0.1)))
    v20_cpi_raw = 1.0 /
<truncated 6989 bytes>
18}")
    print(f"  {'Threads':<25} {X86_THREADS:<18} {V13_THREADS:<18} {V20_THREADS:<18} {V30_THREADS:<18}")
    print(f"  {'Threads/core':<25} {X86_THREADS//PHYSICAL_CORES:<18} {V13_THREADS//PHYSICAL_CORES:<18} {V20_THREADS_PER_CORE:<18} {V30_THREADS_PER_CORE:<18}")
    print(f"  {'Decode (cycles)':<25} {X86_DECODE:<18} {V13_DECODE:<18} {V20_DECODE:<18} {V30_DECODE_EFF:<18.2f}")
    print(f"  {'Fusion multiplier':<25} {'1.0x':<18} {'1.3x':<18} {'1.5x':<18} {'1.6x':<18}")
    print(f"  {'IPC/thread (peak)':<25} {X86_IPC:<18.1f} {V13_IPC:<18.1f} {V20_IPC:<18.1f} {V30_IPC:<18.2f}")
    print(f"  {'L1/thread (raw)':<25} {'16.0 KB':<18} {'4.57 KB':<18} {'8.0 KB':<18} {'6.4 KB':<18}")
    print(f"  {'L0 micro-cache':<25} {'None':<18} {'None':<18} {'1 KB (70% hit)':<18} {'1 KB (70% hit)':<18}")
    print(f"  {'L4 Stacked Cache':<25} {'None':<18} {'None':<18} {'None':<18} {'128 MB (60% hit)':<18}")
    print(f"  {'ROB/thread':<25} {'112 (shared)':<18} {'112 (shared)':<18} {'196 (indep)':<18} {'313 (indep)':<18}")
    print(f"  {'MLP (miss overlap)':<25} {'3.5':<18} {'N/A':<18} {'6.0':<18} {'8.0':<18}")
    print(f"  {'Effective Max BW':<25} {'64.0 GB/s':<18} {'64.0 GB/s':<18} {'64.0 GB/s':<18} {'96.0 GB/s (HMC)':<18}")
    print(f"  {'TDP':<25} {'100 W':<18} {'80 W':<18} {'75 W':<18} {'85 W':<18}")
    print()
    print("-" * 110)

    rows = []
    for name, w in WORKLOADS.items():
        # Model 1: Optimistic
        opt_x86, opt_bemi = run_optimistic(w)
        opt_speedup = opt_bemi / max(opt_x86, 0.001)

        # Model 2: Grounded v1.3
        gr = run_grounded(w)
        gr_speedup = gr["bemi_total"] / max(gr["x86_total"], 0.001)

        # Model 3: v2.0 Scaled Dominance
        v20 = run_v20(w)
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.