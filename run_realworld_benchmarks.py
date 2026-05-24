"""
Bemi BIOS - Extensive Real-World Benchmark Suite
=============================================
Runs a wider range of extensive tests that model real-world performance across
various computing domains. Focuses on comparing x86 native vs Bemi v7.2 Zero-Footprint Singularity.
"""

import sys
import os

# Make sure the tests/ directory is importable
TEST_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests")
sys.path.insert(0, TEST_DIR)

import web_server_db_bench
import video_transcoding_bench
import crypto_hashing_bench
import game_engine_bench
import hft_serial_bench

SECTION = "=" * 70

def section(title):
    print()
    print(SECTION)
    print(f"  REAL-WORLD WORKLOAD: {title}")
    print(SECTION)

def run_all():
    print()
    print("#" * 70)
    print("  BEMI BIOS -- EXTENSIVE REAL-WORLD BENCHMARK SUITE")
    print("  Comparing Native Monolithic x86 vs Bemi v7.2 Singularity")
    print("#" * 70)

    # 1. Web Server & DB
    section("01 - Web Server & Database Backend (High Concurrency & I/O)")
    speedup_web = web_server_db_bench.run()

    # 2. Video Transcoding
    section("02 - Video Transcoding (Media Encoding & AVX/SIMD)")
    speedup_vid = video_transcoding_bench.run()

    # 3. Cryptography
    section("03 - Cryptography (AES-GCM / SHA-256 Hashing)")
    speedup_crypto = crypto_hashing_bench.run()

    # 4. Game Engine
    section("04 - Game Engine (Ray Tracing & BVH Memory Chasing)")
    speedup_game = game_engine_bench.run()

    # 5. HFT Serial
    section("05 - High-Frequency Trading (Ultra-low Latency Serial)")
    speedup_hft = hft_serial_bench.run()

    print()
    print("#" * 70)
    print("  EXTENSIVE BENCHMARK SUITE COMPLETE")
    print("#" * 70)
    print()
    print("  Summary of Speedups (Bemi v7.2 vs x86):")
    print()
    print(f"  Web Server / Database : {speedup_web:.2f}x")
    print(f"  Video Transcoding     : {speedup_vid:.2f}x")
    print(f"  Cryptography          : {speedup_crypto:.2f}x")
    print(f"  Game Engine / BVH     : {speedup_game:.2f}x")
    print(f"  High-Frequency Trading: {speedup_hft:.2f}x")
    print()
    avg_speedup = (speedup_web + speedup_vid + speedup_crypto + speedup_game + speedup_hft) / 5
    print(f"  Average Arithmetic Gain: {avg_speedup:.2f}x")
    print()

if __name__ == "__main__":
    run_all()
