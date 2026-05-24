import time

import os

# Keep the BIOS prototype consistent with the benchmark suite's ground truth.
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from bemi_constants import PHYSICAL_CORES, X86_THREADS, BEMI_THREADS

class BemiFirmware:
    def __init__(self):
        self.ring_minus_1_active = False
        self.dbt_locked_in_cache = False
        self.translation_matrix_ready = False

    def post(self):
        print("[BIOS] Performing Power-On Self Test (POST)...")
        time.sleep(0.5)
        print(f"[BIOS] {PHYSICAL_CORES} physical cores detected ({X86_THREADS} x86 threads baseline).")
        print(f"[BIOS] Weaponized Bemi back-end online: {BEMI_THREADS} execution threads behind x86 decoder clusters.")
        print("[BIOS] L1/L2/L3 caches OK.")

    def detect_os(self):
        print("[BIOS] Scanning boot devices...")
        time.sleep(0.5)
        # Simulating detecting a classic x86 Windows/Linux EFI
        print("[BIOS] Detected: legacy_x86_bootloader.efi")
        return "LEGACY_X86"

    def initialize_weaponized_dbt(self):
        print("[BIOS] Legacy OS Detected. Initializing Ring -1 DBT Translator...")
        time.sleep(0.5)
        self.ring_minus_1_active = True
        self.dbt_locked_in_cache = True
        self.translation_matrix_ready = True
        print("[BIOS] -> Hardware TSO hooks: ENABLED")
        print("[BIOS] -> Macro-Op Fusion pipeline: ENABLED")
        print("[BIOS] -> Shadow APIC & CR3 Paging: ENABLED")
        print("[BIOS] Weaponized x86 Translation Layer is LOCKED beneath OS visibility.")

    def execute_handoff(self):
        if self.translation_matrix_ready:
            print("[BIOS] Handing execution to Legacy OS via Translation Matrix...")
        else:
            print("[BIOS] Handing execution directly to Native OS...")
        print("[BIOS] Booting OS...")

def run_bios_prototype():
    print("="*50)
    print(" BEMI UNIFIED BIOS TEST ENVIRONMENT")
    print("="*50)
    bios = BemiFirmware()
    bios.post()
    os_type = bios.detect_os()
    
    if os_type == "LEGACY_X86":
        bios.initialize_weaponized_dbt()
        
    bios.execute_handoff()
    print("="*50)
    print("[SYSTEM] Legacy OS is now running seamlessly on Bemi Architecture.")

if __name__ == "__main__":
    run_bios_prototype()
