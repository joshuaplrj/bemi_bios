import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
if not os.path.exists(brains_dir):
    print("Brains dir not found")
    exit(1)

found = []
for conv in os.listdir(brains_dir):
    conv_path = os.path.join(brains_dir, conv)
    if not os.path.isdir(conv_path):
        continue
    log_path = os.path.join(conv_path, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
        
    try:
        with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
            for idx, line in enumerate(f):
                if "postasm.nasm" in line.lower():
                    try:
                        step = json.loads(line)
                    except Exception:
                        continue
                    sidx = step.get("step_index")
                    stype = step.get("type")
                    content = step.get("content", "")
                    if "File Path:" in content or "TargetFile" in line:
                        found.append({
                            "conv": conv,
                            "step": sidx or idx,
                            "type": stype,
                            "content": content
                        })
    except Exception as e:
        pass

print(f"Found {len(found)} references in all logs:")
for i, inst in enumerate(found):
    print(f"  {i}: Conv: {inst['conv']} | Step: {inst['step']} | Type: {inst['type']} | Length={len(inst['content'])}")
