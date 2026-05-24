import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"

found_snippets = []

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
                if any(k in line.lower() for k in ["x8088", "pub mod decoder", "pub mod translator", "pub mod optimizer"]):
                    try:
                        step = json.loads(line)
                    except Exception:
                        continue
                    
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
                            code = args.get("CodeContent") or args.get("ReplacementContent") or ""
                            if tf and any(k in tf.lower() for k in ["x8088", "decoder", "translator", "optimizer", "codegen", "executor"]):
                                found_snippets.append({
                                    "conv": conv, "step": sidx or idx, "type": "tool_call:" + tname, "path": tf, "length": len(code)
                                })
                                
                    if stype == "VIEW_FILE" and "File Path:" in content:
                        path_match = re.search(r"File Path:\s*`file:///(.*?)`", content)
                        if path_match:
                            fp = path_match.group(1)
                            if any(k in fp.lower() for k in ["x8088", "decoder", "translator", "optimizer", "codegen", "executor"]):
                                found_snippets.append({
                                    "conv": conv, "step": sidx or idx, "type": "VIEW_FILE", "path": fp, "length": len(content)
                                })
    except Exception as e:
        print(f"Error: {e}")

print(f"Found {len(found_snippets)} snippets:")
for s in found_snippets:
    print(f"Conv: {s['conv']} | Step: {s['step']} | Type: {s['type']} | Path: {s['path']} | Len: {s['length']}")
