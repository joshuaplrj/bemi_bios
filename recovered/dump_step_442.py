import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "43501d54-94a1-43ac-8dd7-7b9742dce801"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

with open(log_path, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            step = json.loads(line)
        except Exception:
            continue
        if step.get("step_index") == 442:
            print("Step 442 Found! Content:")
            print(step.get("content")[:1000])
            print("...")
            print(step.get("content")[-1000:])
            break
