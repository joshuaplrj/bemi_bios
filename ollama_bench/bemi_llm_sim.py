"""
Bemi BIOS LLM Inference Workload Simulator
=========================================
Models the exact hardware-level instruction stream of running Gemma 3
(200M parameters) autoregressive inference on a stock x86 CPU, and
then applies Bemi BIOS v7.2 virtualization to measure the acceleration.

Uses statistical cycle modeling: instead of enumerating billions of
individual instructions, it computes total cycles analytically based
on instruction type distributions and cache behavior.
"""

import math
import random
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "simulator"))


class LLMInferenceWorkload:
    """Models the hardware cost of Gemma 3 200M inference via instruction statistics."""

    MODEL_PARAMS = {
        "gemma3:200m": {
            "param_count": 200_000_000,
            "dtype_bytes": 2,
            "layers": 18,
            "hidden_dim": 1024,
            "intermediate_dim": 4096,
            "num_attention_heads": 8,
            "head_dim": 128,
            "vocab_size": 256000,
        }
    }

    def __init__(self, model_name="gemma3:200m", num_tokens_to_generate=32):
        self.model_name = model_name
        self.num_tokens_to_generate = num_tokens_to_generate
        self.params = self.MODEL_PARAMS.get(model_name, self.MODEL_PARAMS["gemma3:200m"])
        self._compute_layer_stats()

    def _compute_layer_stats(self):
        d = self.params["hidden_dim"]
        d_ff = self.params["intermediate_dim"]
        dt = self.params["dtype_bytes"]
        cache_line = 64

        # Weight memory per layer: Q(1), K(1), V(1), O(1) = 4*d*d, FFN: d*d_ff + d_ff*d = 2*d*d_ff
        self.weight_bytes_per_layer = (4 * d * d + 2 * d * d_ff) * dt
        self.weight_cache_lines_per_layer = self.weight_bytes_per_layer // cache_line

        # Activation memory per layer (KV cache, intermediate activations)
        self.activation_bytes_per_layer = (4 * d * d + 2 * d * d_ff) * dt * 0.1
        self.activation_cache_lines_per_layer = int(self.activation_bytes_per_layer // cache_line)

        # Compute: FLOPs per layer
        self.flops_per_layer = 4 * d * d + 2 * d * d_ff  # MatMul FLOPs
        self.attn_flops_per_layer = d * d * 2  # QK^T + AV
        self.total_flops_per_layer = self.flops_per_layer + self.attn_flops_per_layer
        self.total_flops_per_token = self.total_flops_per_layer * self.params["layers"]

        self.model_size_mb = (self.params["param_count"] * dt) / (1024 * 1024)

    def compute_cycles_per_token_legacy(self):
        """
        Stock Pentium: 200MHz, 4-cycle CISC decode, 40-cycle DRAM latency, 1.6 GB/s.
        Each weight loaded = DRAM access at 40 cycles (cache miss due to 8KB D-cache).
        Each compute FLOP = ~5 x86 instructions * (4 decode + 0.5 exec + 0.5 fetch) = ~25 cycles.
        """
        lines_per_layer = self.weight_cache_lines_per_layer + self.activation_cache_lines_per_layer
        total_mem_accesses = lines_per_layer * self.params["layers"]

        # Memory cycles: virtually all cache misses (8KB cache can't hold 400MB model)
        # Each cache miss: 40 cycles DRAM + 4 decode + 1 exec = 45 cycles
        mem_cycles = total_mem_accesses * 45

        # Compute cycles: ~5 instructions per FLOP
        total_compute_ops = int(self.total_flops_per_token * 5)
        # Each instruction: 4 decode + 0.5 exec + 0.5 fetch = 5 cycles
        compute_cycles = total_compute_ops * 5

        # Branch overhead (loop iterations, attention branches)
        branches = self.params["layers"] * 50
        branch_mispredict_rate = 0.50
        branch_cycles = branches * (0.5 * 1 + 0.5 * 12)  # 50% hit (1c), 50% miss (12c)

        return mem_cycles + compute_cycles + branch_cycles

    def compute_cycles_per_token_bemi(self):
        """
        Bemi v7.2:
        - Memory compression 3x: effective lines = total / 3
        - L0 cache covers 83% of accesses (L0 hit: 1c, L1 hit: 1c, miss: 40c/MLP)
        - MLP-16: effective DRAM latency = 40/16 = 2.5 cycles
        - Memory compression overhead: 2 cycles per compressed line
        - Decode: 0.5 cycles (trace cache, 92% hit)
        - Fusion bonus: 1.45x (fewer ops needed)
        - Branch: NPP predictor 82.5% hit, 4-cycle penalty
        - 8 virtual threads: throughput scaled
        """
        lines_per_layer = self.weight_cache_lines_per_layer + self.activation_cache_lines_per_layer
        # 3x memory compression reduces cache line transfers
        effective_lines = lines_per_layer / 3.0

        total_eff_lines = effective_lines * self.params["layers"]

        # Bemi cache: L0 hit 83%, L1 hit for remaining 0.83, miss for rest
        l0_hit_rate = 0.833
        l1_hit_rate = 0.80
        l0_hits = total_eff_lines * l0_hit_rate
        l1_hits = total_eff_lines * (1 - l0_hit_rate) * l1_hit_rate
        dram_misses = total_eff_lines * (1 - l0_hit_rate) * (1 - l1_hit_rate)

        mlp_factor = 16.0
        effective_dram_latency = 40.0 / mlp_factor
        compression_overhead = 2.0

        # Memory cycles
        mem_cycles = (l0_hits * (1.0 + 0.5 + 0.5)
                      + l1_hits * (1.0 + 0.5 + 0.5)
                      + dram_misses * (effective_dram_latency + compression_overhead + 0.5 + 0.5))

        # Compute cycles: fusion reduces ops by 1.45x, decode reduced to 0.5c
        fusion_bonus = 1.45
        total_compute_ops = int(self.total_flops_per_token * 5 / fusion_bonus)
        compute_cycles = total_compute_ops * (0.5 + 0.5 + 0.5)  # 0.5 decode + 0.5 exec/fusion + 0.5 fetch

        # Branch: NPP 82.5% hit, 4-cycle penalty
        branches = self.params["layers"] * 50
        branch_hit_rate = 0.825
        branch_cycles = branches * (branch_hit_rate * 0.5 + (1 - branch_hit_rate) * 4)

        # Thread scaling: 8 virtual threads share workload
        total_cycles = mem_cycles + compute_cycles + branch_cycles
        scaled_cycles = total_cycles / 8.0

        return scaled_cycles

    def description(self):
        d = self.params["hidden_dim"]
        d_ff = self.params["intermediate_dim"]
        layers = self.params["layers"]
        return (
            f"Gemma 3 200M parameter LLM inference\n"
            f"  Architecture: {layers} layers, d={d}, d_ff={d_ff}\n"
            f"  Model size: {self.model_size_mb:.0f} MB (FP16)\n"
            f"  FLOPs per token: {self.total_flops_per_token / 1e9:.2f} GFLOPs\n"
            f"  Tokens to generate: {self.num_tokens_to_generate}\n"
            f"  Total FLOPs: {self.total_flops_per_token * self.num_tokens_to_generate / 1e9:.2f} GFLOPs"
        )


def run_llm_benchmark(model_name="gemma3:200m", num_tokens=32):
    print("=" * 85)
    print("  BEMI BIOS LLM INFERENCE BENCHMARK")
    print("  Gemma 3 200M Parameter Model - Statistical Hardware Performance")
    print("=" * 85)

    workload = LLMInferenceWorkload(model_name=model_name, num_tokens_to_generate=num_tokens)
    print(f"\n  Model Configuration:")
    print(f"  {workload.description()}")

    # Stock Pentium
    legacy_cycles_per_token = workload.compute_cycles_per_token_legacy()
    legacy_total_cycles = legacy_cycles_per_token * num_tokens
    legacy_time_sec = legacy_total_cycles / 200_000_000  # 200 MHz
    legacy_tps = num_tokens / legacy_time_sec if legacy_time_sec > 0 else 0
    legacy_tdp = 10.0
    legacy_energy = legacy_tdp * legacy_time_sec

    print(f"\n  [LEGACY] Stock Pentium (200MHz, 8KB L1, 40c DRAM)")
    print(f"    Cycles per token:   {legacy_cycles_per_token:,.0f}")
    print(f"    Total cycles:       {legacy_total_cycles:,.0f}")
    print(f"    Elapsed time:       {legacy_time_sec*1000:.0f} ms")
    print(f"    Tokens/sec:         {legacy_tps:.2f}")
    print(f"    Energy:             {legacy_energy:.4f} J")
    print(f"    D-Cache hit rate:   ~0% (8KB cache holds < 0.002% of 400MB model)")

    # Bemi v7.2
    bemi_cycles_per_token = workload.compute_cycles_per_token_bemi()
    bemi_total_cycles = bemi_cycles_per_token * num_tokens
    bemi_time_sec = bemi_total_cycles / 200_000_000
    bemi_tps = num_tokens / bemi_time_sec if bemi_time_sec > 0 else 0
    bemi_tdp = 8.5
    bemi_energy = bemi_tdp * bemi_time_sec

    print(f"\n  [BEMI] Bemi BIOS v7.2 (200MHz, 8 virtual threads, 3x compression, MLP-16)")
    print(f"    Cycles per token:   {bemi_cycles_per_token:,.0f}")
    print(f"    Total cycles:       {bemi_total_cycles:,.0f}")
    print(f"    Elapsed time:       {bemi_time_sec*1000:.0f} ms")
    print(f"    Tokens/sec:         {bemi_tps:.2f}")
    print(f"    Energy:             {bemi_energy:.4f} J")
    print(f"    L0 Cache hit rate:  ~83.3% (128KB L0 cache per core)")

    speedup = legacy_time_sec / max(1e-9, bemi_time_sec)
    energy_savings = legacy_energy / max(1e-9, bemi_energy)

    print(f"\n  {'='*85}")
    print(f"  COMPARISON RESULTS")
    print(f"  {'='*85}")
    print(f"  {'Metric':<35} {'Legacy (Stock x86)':<28} {'Bemi v7.2':<28} {'Speedup':<15}")
    print(f"  {'-'*35:<35} {'-'*28:<28} {'-'*28:<28} {'-'*15:<15}")
    print(f"  {'Elapsed Time (ms)':<35} {legacy_time_sec*1000:<28.0f} {bemi_time_sec*1000:<28.0f} {speedup:<15.2f}x")
    print(f"  {'Tokens/Second':<35} {legacy_tps:<28.2f} {bemi_tps:<28.2f} {speedup:<15.2f}x")
    print(f"  {'Cycles/Token':<35} {legacy_cycles_per_token:<28,.0f} {bemi_cycles_per_token:<28,.0f} {'':<15}")
    print(f"  {'Total Cycles':<35} {legacy_total_cycles:<28,.0f} {bemi_total_cycles:<28,.0f} {'':<15}")
    print(f"  {'Energy (J)':<35} {legacy_energy:<28.4f} {bemi_energy:<28.4f} {energy_savings:<15.2f}x")
    print(f"  {'TDP (W)':<35} {legacy_tdp:<28.1f} {bemi_tdp:<28.1f} {'':<15}")
    print(f"  {'Eff. Mem BW (GB/s)':<35} {1.6:<28.1f} {1.6*3.0*2.0:<28.1f} {'':<15}")

    # Instruction estimates (for report)
    estimated_ops_per_token = int(workload.total_flops_per_token * 5)
    bemi_effective_ops = int(estimated_ops_per_token / 1.45)

    return {
        "model": model_name,
        "model_params": 200_000_000,
        "tokens_generated": num_tokens,
        "total_flops_per_token": workload.total_flops_per_token,
        "total_gflops": workload.total_flops_per_token * num_tokens / 1e9,
        "model_size_mb": workload.model_size_mb,
        "legacy": {
            "time_ms": legacy_time_sec * 1000,
            "tokens_per_second": legacy_tps,
            "cycles_per_token": legacy_cycles_per_token,
            "total_cycles": int(legacy_total_cycles),
            "estimated_ops": estimated_ops_per_token * num_tokens,
            "ipc": estimated_ops_per_token / max(1, legacy_cycles_per_token),
            "energy_joules": legacy_energy,
            "tdp_watts": legacy_tdp,
            "d_cache_hit_rate": 0.0,
            "effective_memory_bw_gbps": 1.6,
        },
        "bemi": {
            "time_ms": bemi_time_sec * 1000,
            "tokens_per_second": bemi_tps,
            "cycles_per_token": bemi_cycles_per_token,
            "total_cycles": int(bemi_total_cycles),
            "estimated_ops": bemi_effective_ops * num_tokens,
            "ipc": bemi_effective_ops / max(1, bemi_cycles_per_token * 8),
            "energy_joules": bemi_energy,
            "tdp_watts": bemi_tdp,
            "d_cache_hit_rate": 0.833,
            "effective_memory_bw_gbps": 1.6 * 3.0,
        },
        "speedup": speedup,
        "energy_savings": energy_savings,
        "effective_memory_bandwidth_legacy_gbps": 1.6,
        "effective_memory_bandwidth_bemi_gbps": 1.6 * 3.0,
    }


if __name__ == "__main__":
    result = run_llm_benchmark()
    import json
    output_path = os.path.join(os.path.dirname(__file__), "bemi_llm_benchmark_result.json")
    with open(output_path, "w") as f:
        json.dump(result, f, indent=2)
    print(f"\n  Results saved to: {output_path}")
