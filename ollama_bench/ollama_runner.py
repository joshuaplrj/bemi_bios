"""
Ollama LLM Inference Runner (Windows)
======================================
Runs the gemma 200M parameter model via Ollama and measures real-world
tokens/second on native hardware. Provides a clean CLI for benchmarking
LLM inference throughput with and without Bemi's simulated acceleration.
"""

import subprocess
import time
import json
import sys
import os
import shutil
import platform
import re
from dataclasses import dataclass, field, asdict
from typing import Optional

OLLAMA_MODEL = "gemma3:200m"
DEFAULT_PROMPTS = [
    "Explain quantum computing in one paragraph.",
    "Write a Python function to sort a list using quicksort.",
    "What are the key differences between TCP and UDP?",
    "Describe how a transformer neural network works at a high level.",
    "Write a short poem about machine learning.",
]

@dataclass
class InferenceResult:
    prompt: str
    prompt_tokens: int
    completion_tokens: int
    total_tokens: int
    total_duration_ms: float
    tokens_per_second: float
    prompt_eval_tokens_per_second: float
    eval_tokens_per_second: float
    model_name: str
    success: bool
    error: str = ""


@dataclass
class BenchmarkRun:
    model: str
    model_size_params: str
    runs: list[InferenceResult] = field(default_factory=list)
    avg_tokens_per_second: float = 0.0
    avg_eval_tokens_per_second: float = 0.0
    avg_prompt_eval_tokens_per_second: float = 0.0
    total_tokens_generated: int = 0
    total_duration_seconds: float = 0.0
    system_info: dict = field(default_factory=dict)

    def to_dict(self):
        return {
            "model": self.model,
            "model_size_params": self.model_size_params,
            "avg_tokens_per_second": self.avg_tokens_per_second,
            "avg_eval_tokens_per_second": self.avg_eval_tokens_per_second,
            "avg_prompt_eval_tokens_per_second": self.avg_prompt_eval_tokens_per_second,
            "total_tokens_generated": self.total_tokens_generated,
            "total_duration_seconds": self.total_duration_seconds,
            "num_runs": len(self.runs),
            "system_info": self.system_info,
            "run_details": [asdict(r) for r in self.runs]
        }


def check_ollama_installed():
    return shutil.which("ollama") is not None


def install_ollama():
    print("  Ollama not found. Installing Ollama...")
    print("  Downloading from https://ollama.com/download/windows")
    installer_url = "https://ollama.com/download/OllamaSetup.exe"
    installer_path = os.path.join(os.environ.get("TEMP", "."), "OllamaSetup.exe")

    import urllib.request
    print(f"  Downloading installer to {installer_path}...")
    urllib.request.urlretrieve(installer_url, installer_path)

    print("  Running installer (this will open a GUI window - follow the prompts)...")
    subprocess.run([installer_path], shell=True)
    print("  Ollama installed. Please ensure the Ollama service is running.")
    print("  If it isn't, start it via: ollama serve")


def pull_model(model_name):
    print(f"  Pulling model: {model_name}...")
    result = subprocess.run(
        ["ollama", "pull", model_name],
        capture_output=True, text=True, timeout=300
    )
    if result.returncode != 0:
        print(f"  ERROR pulling model: {result.stderr}")
        return False
    print(f"  Model {model_name} pulled successfully.")
    return True


def check_model_available(model_name):
    result = subprocess.run(
        ["ollama", "list"],
        capture_output=True, text=True, timeout=30
    )
    return model_name in result.stdout


def run_inference(model_name, prompt, timeout_seconds=120):
    try:
        start = time.time()
        result = subprocess.run(
            ["ollama", "run", model_name, "--verbose", prompt],
            capture_output=True, text=True, timeout=timeout_seconds
        )
        elapsed = time.time() - start

        if result.returncode != 0:
            return InferenceResult(
                prompt=prompt[:80],
                prompt_tokens=0, completion_tokens=0, total_tokens=0,
                total_duration_ms=elapsed * 1000,
                tokens_per_second=0.0,
                prompt_eval_tokens_per_second=0.0,
                eval_tokens_per_second=0.0,
                model_name=model_name,
                success=False,
                error=result.stderr[:200]
            )

        output = result.stderr + result.stdout
        prompt_tokens = 0
        completion_tokens = 0
        prompt_eval_rate = 0.0
        eval_rate = 0.0

        for line in output.split("\n"):
            m = re.search(r"prompt eval count:\s*(\d+)", line)
            if m: prompt_tokens = int(m.group(1))
            m = re.search(r"eval count:\s*(\d+)", line)
            if m: completion_tokens = int(m.group(1))
            m = re.search(r"prompt eval rate:\s*([\d.]+)\s*tokens/s", line)
            if m: prompt_eval_rate = float(m.group(1))
            m = re.search(r"eval rate:\s*([\d.]+)\s*tokens/s", line)
            if m: eval_rate = float(m.group(1))

        total = prompt_tokens + completion_tokens
        tps = total / elapsed if elapsed > 0 else 0.0

        return InferenceResult(
            prompt=prompt[:80],
            prompt_tokens=prompt_tokens,
            completion_tokens=completion_tokens,
            total_tokens=total,
            total_duration_ms=elapsed * 1000,
            tokens_per_second=tps,
            prompt_eval_tokens_per_second=prompt_eval_rate,
            eval_tokens_per_second=eval_rate,
            model_name=model_name,
            success=True
        )

    except subprocess.TimeoutExpired:
        return InferenceResult(
            prompt=prompt[:80], prompt_tokens=0, completion_tokens=0,
            total_tokens=0, total_duration_ms=timeout_seconds * 1000,
            tokens_per_second=0.0, prompt_eval_tokens_per_second=0.0,
            eval_tokens_per_second=0.0, model_name=model_name,
            success=False, error="Timeout expired"
        )
    except Exception as e:
        return InferenceResult(
            prompt=prompt[:80], prompt_tokens=0, completion_tokens=0,
            total_tokens=0, total_duration_ms=0,
            tokens_per_second=0.0, prompt_eval_tokens_per_second=0.0,
            eval_tokens_per_second=0.0, model_name=model_name,
            success=False, error=str(e)[:200]
        )


def get_system_info():
    info = {
        "platform": platform.platform(),
        "processor": platform.processor(),
        "python_version": platform.python_version(),
        "machine": platform.machine()
    }
    try:
        import psutil
        info["cpu_count"] = psutil.cpu_count(logical=True)
        info["cpu_physical_cores"] = psutil.cpu_count(logical=False)
        mem = psutil.virtual_memory()
        info["total_ram_gb"] = round(mem.total / (1024 ** 3), 2)
        info["available_ram_gb"] = round(mem.available / (1024 ** 3), 2)
    except ImportError:
        info["psutil_available"] = False
    return info


def run_benchmark(model_name=OLLAMA_MODEL, prompts=None, warmup=True):
    if prompts is None:
        prompts = DEFAULT_PROMPTS

    print("=" * 70)
    print("  OLLAMA LLM INFERENCE BENCHMARK - NATIVE HARDWARE")
    print("=" * 70)

    sys_info = get_system_info()
    print(f"\n  System: {sys_info['platform']}")
    print(f"  Processor: {sys_info['processor']}")
    if "total_ram_gb" in sys_info:
        print(f"  RAM: {sys_info['total_ram_gb']} GB total, "
              f"{sys_info['available_ram_gb']} GB available")
    if "cpu_count" in sys_info:
        print(f"  CPU Cores: {sys_info['cpu_physical_cores']} physical, "
              f"{sys_info['cpu_count']} logical")

    if not check_ollama_installed():
        install_ollama()
        if not check_ollama_installed():
            print("  ERROR: Ollama installation failed. Cannot proceed.")
            return None

    if not check_model_available(model_name):
        pull_model(model_name)
        if not check_model_available(model_name):
            print(f"  ERROR: Model {model_name} not available. Cannot proceed.")
            return None

    print(f"\n  Model: {model_name}")
    print(f"  Number of prompts: {len(prompts)}")
    print(f"  Warmup run: {'Yes' if warmup else 'No'}")
    print()

    if warmup:
        print("  [WARMUP] Running initial inference to prime model...")
        run_inference(model_name, "Hello.", timeout_seconds=60)
        print("  Warmup complete.\n")

    results = []
    for i, prompt in enumerate(prompts):
        print(f"  [{i+1}/{len(prompts)}] Prompt: {prompt[:60]}...")
        result = run_inference(model_name, prompt)
        if result.success:
            print(f"    -> {result.eval_tokens_per_second:.1f} eval tok/s | "
                  f"{result.prompt_eval_tokens_per_second:.1f} prompt tok/s | "
                  f"{result.completion_tokens} completion tokens")
        else:
            print(f"    -> FAILED: {result.error[:80]}")
        results.append(result)

    successful = [r for r in results if r.success]
    if not successful:
        print("\n  ERROR: All inference runs failed.")
        return None

    avg_eval_tps = sum(r.eval_tokens_per_second for r in successful) / len(successful)
    avg_prompt_tps = sum(r.prompt_eval_tokens_per_second for r in successful) / len(successful)
    avg_tps = sum(r.tokens_per_second for r in successful) / len(successful)
    total_tokens = sum(r.completion_tokens for r in successful)
    total_duration = sum(r.total_duration_ms for r in successful) / 1000.0

    bench = BenchmarkRun(
        model=model_name,
        model_size_params="200M",
        runs=results,
        avg_tokens_per_second=avg_tps,
        avg_eval_tokens_per_second=avg_eval_tps,
        avg_prompt_eval_tokens_per_second=avg_prompt_tps,
        total_tokens_generated=total_tokens,
        total_duration_seconds=total_duration,
        system_info=sys_info
    )

    print(f"\n  {'='*70}")
    print(f"  BENCHMARK RESULTS (Native Hardware)")
    print(f"  {'='*70}")
    print(f"  Avg eval tokens/sec:      {avg_eval_tps:.2f}")
    print(f"  Avg prompt eval tok/sec:   {avg_prompt_tps:.2f}")
    print(f"  Avg overall tokens/sec:     {avg_tps:.2f}")
    print(f"  Total completion tokens:    {total_tokens}")
    print(f"  Total duration:             {total_duration:.2f}s")
    print(f"  Successful runs:            {len(successful)}/{len(results)}")

    return bench


if __name__ == "__main__":
    result = run_benchmark()
    if result:
        output_path = os.path.join(os.path.dirname(__file__), "ollama_benchmark_result.json")
        with open(output_path, "w") as f:
            json.dump(result.to_dict(), f, indent=2)
        print(f"\n  Results saved to: {output_path}")
