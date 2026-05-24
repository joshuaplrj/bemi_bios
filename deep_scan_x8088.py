import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
line_num_pattern = re.compile(r"^(\s*\d+):\s(.*)$")

def clean_view_content(content):
    lines = content.splitlines()
    code_lines = []
    started = False
    for line in lines:
        if "The following code has been modified to include a line number" in line:
            started = True
            continue
        if not started:
            continue
        m = line_num_pattern.match(line)
        if m:
            code_lines.append(m.group(2))
        elif line.strip() == "The above content shows the entire, complete file contents of the requested file.":
            break
        elif started:
            code_lines.append(line)
    if code_lines:
        return "\n".join(code_lines)
    return None

results = []

for conv in os.listdir(brains_dir):
    conv_path = os.path.join(brains_dir, conv)
    if not os.path.isdir(conv_path):
        continue
    log_path = os.path.join(conv_path, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
    try:
        with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
            for idx, line_raw in enumerate(f):
                # Search for target files
                if "x8088" in line_raw.lower() or "sim8088" in line_raw.lower():
                    try:
                        step = json.loads(line_raw)
                    except:
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
                                code = args.get("CodeContent") or args.get("ReplacementContent") or ""
                                if tf_path:
                                    is_tr = "<truncated" in code
                                    results.append({
                                        "type": "write",
                                        "file": tf_path,
                                        "len": len(code),
                                        "conv": conv,
                                        "step": sidx or idx,
                                        "truncated": is_tr,
                                        "code": code
                                    })
                                    
                    if stype == "VIEW_FILE" and "File Path:" in content:
                        path_match = re.search(r"File Path:\s*`file:///(.*?)`", content)
                        if path_match:
                            file_path = path_match.group(1).replace("%20", " ")
                            code = clean_view_content(content)
                            if code:
                                is_tr = "<truncated" in content or "<truncated" in code
                                results.append({
                                    "type": "view",
                                    "file": file_path,
                                    "len": len(code),
                                    "conv": conv,
                                    "step": sidx or idx,
                                    "truncated": is_tr,
                                    "code": code
                                })
    except Exception as e:
         pass

print(f"Found {len(results)} matches for x8088/sim8088 in logs:")
for r in results:
    print(f"Type: {r['type']} | File: {r['file']} | Len: {r['len']} | Truncated: {r['truncated']} | Conv: {r['conv']} | Step: {r['step']}")
    if "x8088.rs" in r['file'].lower() and not r['truncated']:
        print("FOUND UNTRUNCATED x8088.rs!")
        dest = os.path.join(brains_dir, "x8088_recovered.rs")
        with open(dest, "w", encoding="utf-8") as f:
            f.write(r['code'])
        print(f"Wrote to {dest}")
