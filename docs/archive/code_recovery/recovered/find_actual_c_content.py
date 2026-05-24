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
    
    print(f"\nChecking {conv}...")
    with open(log_path, 'r', encoding='utf-8') as f:
        for idx, line in enumerate(f):
            if "BemiBiosEntryPoint" in line:
                try:
                    step = json.loads(line)
                except Exception:
                    continue
                content = step.get("content", "")
                sidx = step.get("step_index")
                stype = step.get("type")
                
                # Check if it has File Path
                m = path_pattern.search(content)
                path_str = m.group(1) if m else "Unknown path"
                
                # Let's see if this is BemiBiosCore.c or BemiBiosCore.inf
                is_c = path_str.lower().endswith(".c")
                is_truncated = "<truncated" in content
                
                print(f"  Step {sidx or idx} | Type: {stype} | Path: {path_str} | Is C: {is_c} | Length: {len(content)} | Truncated: {is_truncated}")
