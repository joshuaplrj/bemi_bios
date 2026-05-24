import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "43501d54-94a1-43ac-8dd7-7b9742dce801"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

steps_to_dump = [163, 520]

with open(log_path, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            step = json.loads(line)
        except Exception:
            continue
        sidx = step.get("step_index")
        if sidx in steps_to_dump:
            print(f"\n--- Step {sidx} ---")
            print(step.get("content"))
