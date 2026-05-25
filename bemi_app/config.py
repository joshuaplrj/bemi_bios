"""
Bemi App Configuration
=====================
"""
import os
import json

APP_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(APP_DIR, "config.json")
OLLAMA_BASE_URL = "http://localhost:11434"
DEFAULT_MODEL = "gemma3:200m"
MODELS = ["gemma3:200m", "gemma3:1b", "llama3.2:1b", "phi3:mini", "qwen2.5:0.5b"]

DEFAULT_PROMPTS = [
    "Explain quantum computing in one paragraph.",
    "Write a Python function to sort a list using quicksort.",
    "What are the key differences between TCP and UDP?",
    "Describe how a transformer neural network works at a high level.",
    "Write a short poem about machine learning.",
]

DEFAULT_CONFIG = {
    "model": DEFAULT_MODEL,
    "ollama_port": 11434,
    "auto_start_ollama": True,
    "warmup_runs": 1,
    "benchmark_runs": 5,
    "apply_optimizations": True,
    "cpu_high_priority": True,
    "lock_to_performance_cores": True,
    "enable_large_pages": True,
    "num_threads": 0,
    "numa_aware": True,
    "keep_ollama_running": False,
}


def load_config():
    if os.path.exists(CONFIG_PATH):
        with open(CONFIG_PATH) as f:
            cfg = json.load(f)
            merged = {**DEFAULT_CONFIG, **cfg}
            return merged
    return dict(DEFAULT_CONFIG)


def save_config(cfg):
    with open(CONFIG_PATH, "w") as f:
        json.dump(cfg, f, indent=2)
