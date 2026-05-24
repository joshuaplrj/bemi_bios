import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"

found_files = {}

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
                if ".rs" not in line.lower():
                    continue
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
                    if tname in ["write_to_file", "replace_file_content", "multi_replace_file_content"]:
                        args = tc.get("arguments", tc.get("args", {}))
                        if isinstance(args, str):
                            try: args = json.loads(args)
                            except: pass
                        if isinstance(args, dict):
                            tf_path = args.get("TargetFile") or ""
                            if tf_path and tf_path.lower().endswith(".rs"):
                                is_tr = "<truncated" in (args.get("CodeContent") or args.get("ReplacementContent") or "")
                                key = tf_path.lower().replace("\\", "/")
                                if key not in found_files or (found_files[key]["truncated"] and not is_tr):
                                    found_files[key] = {"conv": conv, "step": sidx or idx, "truncated": is_tr, "path": tf_path}
                                    
                if stype == "VIEW_FILE" and "File Path:" in content:
                    path_match = re.search(r"File Path:\s*`file:///(.*?)`", content)
                    if path_match:
                        file_path = path_match.group(1).replace("%20", " ")
                        if file_path.lower().endswith(".rs"):
                            key = file_path.lower().replace("\\", "/")
                            is_tr = "<truncated" in content
                            if key not in found_files or (found_files[key]["truncated"] and not is_tr):
                                found_files[key] = {"conv": conv, "step": sidx or idx, "truncated": is_tr, "path": file_path}
    except Exception as e:
        print(f"Error reading conv {conv}: {e}")

print("Found Rust files in logs:")
for k, v in found_files.items():
    print(f"Path: {v['path']} | Conv: {v['conv']} | Step: {v['step']} | Truncated: {v['truncated']}")
