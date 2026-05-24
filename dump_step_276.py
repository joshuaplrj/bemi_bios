import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "cf3c8394-0a7b-4e70-acf5-b3bbbb227053"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

with open(log_path, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            step = json.loads(line)
        except Exception:
            continue
        if step.get("step_index") == 276:
            print("Step 276 content:")
            print(step.get("content")[:1000])
            break
