# run_pentium_validations.py
"""
Bemi BIOS v7.2 - Pentium CPU & Apt OS Validations Proxy Runner
=============================================================
Forwards execution to the reorganized simulator script.
"""

import sys
import os

# Add simulator folder to python path so it can resolve local imports correctly
SIM_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "simulator"))
sys.path.insert(0, SIM_DIR)

if __name__ == "__main__":
    from simulator.run_pentium_validations import run_validations
    run_validations()
