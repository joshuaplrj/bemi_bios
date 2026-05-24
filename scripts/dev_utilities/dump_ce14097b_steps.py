import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "ce14097b-5bff-4a33-87e9-c3d8b2c235a3"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

steps_to_dump = [75, 76, 77, 78]

with open(log_path, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            step = json.loads(line)
        except Exception:
            continue
        sidx = step.get("step_index")
        if sidx in steps_to_dump:
            print(f"\n--- Step {sidx} ---")
            content = step.get("content", "")
            print(content[:500]) # Print first 500 chars of each
