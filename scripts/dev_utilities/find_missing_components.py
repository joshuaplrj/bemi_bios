import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
path_pattern = re.compile(r"File Path:\s*`file:///(.*?)`", re.IGNORECASE)

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
                if "hypervisor" in line.lower() or "legacy" in line.lower():
                    # Check if it has File Path or write_to_file
                    try:
                        step = json.loads(line)
                    except Exception:
                        continue
                    
                    sidx = step.get("step_index")
                    stype = step.get("type")
                    content = step.get("content", "")
                    
                    # 1. Check tool calls
                    tool_calls = step.get("tool_calls", [])
                    for tc in tool_calls:
                        args = tc.get("arguments", tc.get("args", {}))
                        if isinstance(args, str):
                            try: args = json.loads(args)
                            except: pass
                        if isinstance(args, dict):
                            tf = args.get("TargetFile") or args.get("AbsolutePath")
                            if tf and ("hypervisor" in tf.lower() or "legacy" in tf.lower()):
                                code = args.get("CodeContent") or args.get("ReplacementContent") or ""
                                found.append({
                                    "conv": conv,
                                    "step": sidx or idx,
                                    "type": "tool_call:" + tc.get("name"),
                                    "path": tf,
                                    "length": len(code),
                                    "content": code,
                                    "truncated": "<truncated" in code
                                })
                    
                    # 2. Check VIEW_FILE
                    if stype == "VIEW_FILE" and "File Path:" in content:
                        m = path_pattern.search(content)
                        if m and ("hypervisor" in m.group(1).lower() or "legacy" in m.group(1).lower()):
                            is_tr = "<truncated" in content
                            found.append({
                                "conv": conv,
                                "step": sidx or idx,
                                "type": "VIEW_FILE",
                                "path": m.group(1),
                                "length": len(content),
                                "content": content,
                                "truncated": is_tr
                            })
    except Exception as e:
        pass

print(f"Found {len(found)} instances of hypervisor/legacy files:")
for i, inst in enumerate(found):
    # Print clean info
    path_clean = inst['path'].replace("%20", " ")
    print(f"  {i}: Conv: {inst['conv']} | Step: {inst['step']} | Type: {inst['type']} | Path: {path_clean} | Len: {inst['length']} | Truncated: {inst['truncated']}")
