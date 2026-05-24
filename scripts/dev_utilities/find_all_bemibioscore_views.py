import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conversations = [
    "43501d54-94a1-43ac-8dd7-7b9742dce801",
    "cf3c8394-0a7b-4e70-acf5-b3bbbb227053",
    "b8be5ad8-2723-4f7f-a057-27cef6eac548"
]

path_pattern = re.compile(r"File Path:\s*`file:///(.*?)`", re.IGNORECASE)

for conv in conversations:
    log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
    
    print(f"\nSearching {conv}...")
    with open(log_path, 'r', encoding='utf-8') as f:
        for idx, line in enumerate(f):
            if "File Path:" in line:
                try:
                    step = json.loads(line)
                except Exception:
                    continue
                content = step.get("content", "")
                m = path_pattern.search(content)
                if m:
                    file_path = m.group(1)
                    if file_path.lower().endswith("bemibioscore.c"):
                        sidx = step.get("step_index")
                        is_truncated = "<truncated" in content
                        print(f"  Step {sidx or idx}: {file_path} | Length={len(content)} | Truncated={is_truncated}")
