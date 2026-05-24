import math

PHYSICAL_CORES = 12
CLOCK_FREQ_GHZ = 3.5
MEM_INST_FRACTION = 0.25
MEM_MISS_LATENCY = 40
L1_HIT_LATENCY = 4
L1_PER_CORE_KB = 32
PEAK_MEM_BANDWIDTH_GBS = 64.0

WORKLOADS = {
    "DL Training": {
        "ipc_max": 3.2, "k_sat": 256, "cycles_per_op": 8, "serial_pct": 0.01, "risc_expansion": 1.3, "hmc_compression": 2.2
    },
    "DPDK Packet Processing": {
        "ipc_max": 1.8, "k_sat": 128, "cycles_per_op": 12, "serial_pct": 0.05, "risc_expansion": 1.3, "hmc_compression": 1.5
    },
    "Ray Tracing": {
        "ipc_max": 1.4, "k_sat": 96, "cycles_per_op": 14, "serial_pct": 0.15, "risc_expansion": 1.3, "hmc_compression": 1.5
    },
    "Garbage Collection": {
        "ipc_max": 0.6, "k_sat": 32, "cycles_per_op": 20, "serial_pct": 0.60, "risc_expansion": 1.5, "hmc_compression": 1.5
    },
    "Video Encoding": {
        "ipc_max": 2.8, "k_sat": 160, "cycles_per_op": 6, "serial_pct": 0.03, "risc_expansion": 1.2, "hmc_compression": 1.8
    },
    "OLAP Scan": {
        "ipc_max": 2.0, "k_sat": 192, "cycles_per_op": 10, "serial_pct": 0.02, "risc_expansion": 1.1, "hmc_compression": 2.0
    },
    "HFT Serial": {
        "ipc_max": 1.0, "k_sat": 48, "cycles_per_op": 4, "serial_pct": 0.50, "risc_expansion": 1.0, "hmc_compression": 1.5
    },
    "SHA-256 Hashing": {
        "ipc_max": 0.8, "k_sat": 48, "cycles_per_op": 5, "serial_pct": 0.35, "risc_expansion": 1.0, "hmc_compression": 1.5
    },
    "Bioinformatics": {
       
<truncated 4636 bytes>
t_rate * 1 +
                   l1_access_fraction * (1 - v40_miss / l1_access_fraction) * L1_HIT_LATENCY +
                   v40_miss * v40_eff_mem_lat)
                   
    v40_ipc_raw = w["ipc_max"] * (1.0 - math.exp(-v40_rob_per_thread / w["k_sat"]))
    v40_stall = max(0.0, v40_avg_mem - (v40_rob_per_thread / max(v40_ipc_raw, 0.1)))
    v40_ipc_eff = 1.0 / (1.0 / max(v40_ipc_raw, 0.1) + MEM_INST_FRACTION * v40_stall)
    
    # 10-pair fusion + Neural branch predictor speedup
    v40_ipc_eff *= (4.0 / V40_DECODE_EFF) * V40_FUSION
    
    # Amdahl's law
    v40_speedup = amdahl_correct(w["serial_pct"], v40_threads)
    v40_tp = v40_ipc_eff / w["cycles_per_op"] * v40_speedup / w["risc_expansion"]
    
    # Adaptive HMC Compression
    hmc_ratio = w["hmc_compression"]
    v40_peak_bw = PEAK_MEM_BANDWIDTH_GBS * hmc_ratio
    
    v40_mem_req = v40_tp * MEM_INST_FRACTION * CLOCK_FREQ_GHZ * 8.0
    bw_limit = v40_peak_bw * V40_BW_THROTTLE_PCT
    v40_bw_scale = min(1.0, bw_limit / max(v40_mem_req, 0.001))
    return v40_tp * v40_bw_scale

print("Workload                    x86     v3.0    v4.0    v3.0 Speedup  v4.0 Speedup")
print("-------------------------------------------------------------------------------")
sum_v30 = 0.0
sum_v40 = 0.0
for name, w in WORKLOADS.items():
    x86_val = run_x86(w)
    v30_val = run_v30(w)
    v40_val = run_v40(w)
    s30 = v30_val / x86_val
    s40 = v40_val / x86_val
    sum_v30 += s30
    sum_v40 += s40
    print(f"{name:<27} {x86_val:.3f}   {v30_val:.3f}   {v40_val:.3f}   {s30:>11.2f}x   {s40:>11.2f}x")

print("-------------------------------------------------------------------------------")
print(f"AVERAGE                                                 {sum_v30/10:.2f}x   {sum_v40/10:.2f}x")
