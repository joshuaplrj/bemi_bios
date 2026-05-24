"""
Bemi BIOS - Expanded Multi-Domain Benchmark Suite Runner
======================================================
Runs the 13 specialized multi-domain benchmarks comparing
native monolithic x86 vs Bemi v7.2 Zero-Footprint Singularity.
"""
import sys
import os

# Make sure the tests/ directory is importable
TEST_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests")
sys.path.insert(0, TEST_DIR)

import video_editing_bench
import video_encoding_bench
import video_decoding_bench
import encryption_bench
import decryption_bench
import python_programming_bench
import swift_programming_bench
import c_programming_bench
import go_programming_bench
import javascript_programming_bench
import ai_neural_net_bench
import ai_basic_principles_bench
import fuzzy_logic_bench

SECTION = "=" * 70

def section(title):
    print()
    print(SECTION)
    print(f"  WORKLOAD: {title}")
    print(SECTION)

def run_all():
    print()
    print("#" * 70)
    print("  BEMI BIOS -- EXPANDED MULTI-DOMAIN BENCHMARK SUITE")
    print("  Comparing Native Monolithic x86 vs Bemi v7.2 Singularity")
    print("#" * 70)

    # 1. Video Pipeline
    section("01 - Video Editing Timeline")
    su_editing = video_editing_bench.run()

    section("02 - Video Encoding")
    su_encoding = video_encoding_bench.run()

    section("03 - Video Decoding")
    su_decoding = video_decoding_bench.run()

    # 2. Cryptographic Security
    section("04 - Symmetric Encryption")
    su_encryption = encryption_bench.run()

    section("05 - Decryption & Asymmetric Cryptography")
    su_decryption = decryption_bench.run()

    # 3. Programming Languages
    section("06 - Python Interpreter & GIL")
    su_python = python_programming_bench.run()

    section("07 - Swift Runtime (ARC & COW)")
    su_swift = swift_programming_bench.run()

    section("08 - C Performance (Pointer Arithmetic & Loops)")
    su_c = c_programming_bench.run()

    section("09 - Go Runtime (Goroutines & GC Pause)")
    su_go = go_programming_bench.run()

    section("10 - Javascript Event Loop")
    su_javascript = javascript_programming_bench.run()

    # 4. AI & Decision Engines
    section("11 - AI Neural Network Training")
    su_nn = ai_neural_net_bench.run()

    section("12 - AI Basic Principles Training")
    su_basic_ml = ai_basic_principles_bench.run()

    section("13 - Fuzzy Logic Inference System")
    su_fuzzy = fuzzy_logic_bench.run()

    # Consolidate results
    results = [
        ("Video Editing Timeline", su_editing),
        ("Video Encoding", su_encoding),
        ("Video Decoding", su_decoding),
        ("Symmetric Encryption", su_encryption),
        ("Decryption & Asymmetric Crypto", su_decryption),
        ("Python Interpreter (GIL Limit)", su_python),
        ("Swift Runtime (ARC & COW)", su_swift),
        ("Compiled C Performance", su_c),
        ("Go Runtime Concurrency & GC", su_go),
        ("Javascript Event Loop", su_javascript),
        ("AI Neural Network Training", su_nn),
        ("AI Basic Principles Training", su_basic_ml),
        ("Fuzzy Logic Rule Inference", su_fuzzy),
    ]

    print()
    print("#" * 70)
    print("  EXPANDED MULTI-DOMAIN BENCHMARK COMPLETE")
    print("#" * 70)
    print()
    print("  Summary of Speedups (Bemi v7.2 vs x86 Baseline):")
    print("  " + "-" * 56)
    print(f"  {'Workload Domain':<35} | {'Speedup Factor':<18}")
    print("  " + "-" * 56)
    for name, su in results:
        print(f"  {name:<35} | {su:>14.2f}x")
    print("  " + "-" * 56)
    
    speedups = [su for _, su in results]
    avg_su = sum(speedups) / len(speedups)
    print(f"  {'Average Arithmetic Gain':<35} | {avg_su:>14.2f}x")
    print("  " + "-" * 56)
    print()

if __name__ == "__main__":
    run_all()
