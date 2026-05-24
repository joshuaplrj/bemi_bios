# run_all_benchmarks.py
"""
Bemi BIOS - Complete Benchmark Suite Proxy Runner
===================================================
Forwards execution to the reorganized tests/benchmarks suite.
"""

import sys
import os

# Add tests/benchmarks/ folder to python path so it can resolve local imports correctly
BENCH_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "tests", "benchmarks"))
sys.path.insert(0, BENCH_DIR)

if __name__ == "__main__":
    from run_all_benchmarks import run_all
    run_all()
