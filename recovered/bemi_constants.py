# bemi_constants compatibility wrapper
import importlib.util
import os
import sys

# Locate the actual bemi_constants.py
target_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "bemi_constants.py"))

if not os.path.exists(target_path):
    # Fallback to the archived copy if parent is not accessible
    target_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "docs", "archive", "code_recovery", "recovered", "bemi_constants.py"))

if os.path.exists(target_path):
    spec = importlib.util.spec_from_file_location("real_bemi_constants", target_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # Expose all non-private attributes to this module's namespace
    for name in dir(module):
        if not name.startswith('_'):
            globals()[name] = getattr(module, name)
else:
    raise ImportError(f"Could not locate the real bemi_constants.py at {target_path}")

