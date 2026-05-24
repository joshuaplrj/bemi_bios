import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conversations = [
    "43501d54-94a1-43ac-8dd7-7b9742dce801",
    "cf3c8394-0a7b-4e70-acf5-b3bbbb227053",
    "b8be5ad8-2723-4f7f-a057-27cef6eac548"
]

for conv in conversations:
    log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
    print(f"\nChecking {conv}...")
    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
        for idx, line in enumerate(f):
            if "bemiprotocol.h" in line.lower() or "bemiprotocol.c" in line.lower():
                try:
                    step = json.loads(line)
                except Exception:
                    continue
                sidx = step.get("step_index")
                stype = step.get("type")
                print(f"  Step {sidx or idx} | Type: {stype} | Len: {len(line)}")
