import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"

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
                if "sim8088.rs" in line.lower():
                    try:
                        step = json.loads(line)
                        sidx = step.get("step_index")
                        stype = step.get("type")
                        content = step.get("content", "")
                        tool_calls = step.get("tool_calls", [])
                        for tc in tool_calls:
                            tname = tc.get("name")
                            args = tc.get("arguments", tc.get("args", {}))
                            if isinstance(args, str):
                                try: args = json.loads(args)
                                except: pass
                            if isinstance(args, dict):
                                tf = args.get("TargetFile") or ""
                                if tf and "sim8088.rs" in tf.lower():
                                    code = args.get("CodeContent") or args.get("ReplacementContent") or ""
                                    found.append((conv, sidx or idx, "tool:" + tname, code))
                        if stype == "VIEW_FILE" and "sim8088.rs" in content.lower():
                            found.append((conv, sidx or idx, "view", content))
                    except Exception:
                        pass
    except Exception as e:
        print(f"Error: {e}")

print(f"Found {len(found)} instances of sim8088.rs:")
for conv, step, ty, code in found:
    print(f"Conv: {conv} | Step: {step} | Type: {ty} | Len: {len(code)}")
    if not "<truncated" in code:
        print("NON-TRUNCATED CODE:")
        print(code[:1000])
        print("...")
