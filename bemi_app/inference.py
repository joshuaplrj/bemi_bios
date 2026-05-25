"""
Ollama Inference Engine
=======================
Runs LLM inference via Ollama REST API with real-time
token/second measurement and progress callback support.
"""
import subprocess
import time
import re
import threading
import json
import urllib.request
import urllib.error


def run_inference_sync(model, prompt, timeout=120, base_url="http://localhost:11434"):
    """Synchronous inference via `ollama run --verbose`."""
    start = time.time()
    try:
        result = subprocess.run(
            ["ollama", "run", model, "--verbose", prompt],
            capture_output=True, text=True,
            timeout=timeout,
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        elapsed = time.time() - start

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

        return {
            "success": result.returncode == 0,
            "prompt": prompt[:100],
            "prompt_tokens": prompt_tokens,
            "completion_tokens": completion_tokens,
            "total_tokens": prompt_tokens + completion_tokens,
            "elapsed_ms": elapsed * 1000,
            "eval_tokens_per_second": eval_rate,
            "prompt_eval_tokens_per_second": prompt_eval_rate,
            "overall_tokens_per_second": (prompt_tokens + completion_tokens) / elapsed if elapsed > 0 else 0,
            "response": result.stdout[:500] if result.returncode == 0 else "",
            "error": result.stderr[:200] if result.returncode != 0 else "",
        }
    except subprocess.TimeoutExpired:
        return {"success": False, "error": "Timeout", "elapsed_ms": timeout * 1000}
    except Exception as e:
        return {"success": False, "error": str(e)[:200]}


def run_inference_api(model, prompt, timeout=120, base_url="http://localhost:11434"):
    """Inference via Ollama REST API with streaming."""
    start = time.time()
    try:
        payload = json.dumps({
            "model": model,
            "prompt": prompt,
            "stream": False,
        }).encode("utf-8")

        req = urllib.request.Request(
            f"{base_url}/api/generate",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))

        elapsed = time.time() - start
        eval_count = data.get("eval_count", 0)
        prompt_eval_count = data.get("prompt_eval_count", 0)
        eval_rate = eval_count / elapsed if elapsed > 0 else 0

        return {
            "success": True,
            "prompt": prompt[:100],
            "prompt_tokens": prompt_eval_count,
            "completion_tokens": eval_count,
            "total_tokens": prompt_eval_count + eval_count,
            "elapsed_ms": elapsed * 1000,
            "eval_tokens_per_second": eval_rate,
            "prompt_eval_tokens_per_second": prompt_eval_count / elapsed if elapsed > 0 else 0,
            "overall_tokens_per_second": (prompt_eval_count + eval_count) / elapsed if elapsed > 0 else 0,
            "response": data.get("response", "")[:500],
        }
    except Exception as e:
        return {"success": False, "error": str(e)[:200]}


def run_benchmark(model, prompts, warmup=True, progress_callback=None, base_url="http://localhost:11434"):
    """Run a full benchmark across multiple prompts."""
    results = []

    if warmup:
        if progress_callback:
            progress_callback("Running warmup...")
        run_inference_sync(model, "Hello.", timeout=60, base_url=base_url)

    total = len(prompts)
    for i, prompt in enumerate(prompts):
        if progress_callback:
            progress_callback(f"Benchmark {i+1}/{total}: {prompt[:50]}...")
        r = run_inference_sync(model, prompt, timeout=120, base_url=base_url)
        results.append(r)

    successful = [r for r in results if r.get("success")]
    if not successful:
        return {"success": False, "error": "All runs failed", "runs": results}

    avg_eval = sum(r["eval_tokens_per_second"] for r in successful) / len(successful)
    avg_prompt = sum(r["prompt_eval_tokens_per_second"] for r in successful) / len(successful)
    avg_overall = sum(r["overall_tokens_per_second"] for r in successful) / len(successful)
    total_completion = sum(r["completion_tokens"] for r in successful)
    total_time = sum(r["elapsed_ms"] for r in successful) / 1000.0

    return {
        "success": True,
        "model": model,
        "avg_eval_tokens_per_second": avg_eval,
        "avg_prompt_tokens_per_second": avg_prompt,
        "avg_overall_tokens_per_second": avg_overall,
        "total_completion_tokens": total_completion,
        "total_duration_seconds": total_time,
        "num_runs": len(results),
        "num_successful": len(successful),
        "runs": results,
    }
