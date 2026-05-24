import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
workspace_dir = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"

dbt_targets = {
    "codegen/mod.rs": "codegen_mod.rs",
    "executor/mod.rs": "executor_mod.rs",
    "ir.rs": "ir_rs",
    "bin/sim8088.rs": "sim8088_rs"
}

best_dbt = {k: {"len": 0, "content": None, "conv": None, "step": None, "truncated": True} for k in dbt_targets}

line_num_pattern = re.compile(r"^(\s*\d+):\s(.*)$")

def clean_view_content(content):
    lines = content.splitlines()
    code_lines = []
    started = False
    for line in lines:
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

for conv in os.listdir(brains_dir):
    conv_path = os.path.join(brains_dir, conv)
    if not os.path.isdir(conv_path):
        continue
    log_path = os.path.join(conv_path, ".system_generated", "logs", 
<truncated 2442 bytes>
       if stype == "VIEW_FILE" and "File Path:" in content:
                    path_match = re.search(r"File Path:\s*`file:///(.*?)`", content)
                    if path_match:
                        file_path = path_match.group(1).replace("%20", " ")
                        fp_lower = file_path.lower().replace("\\", "/")
                        for target in dbt_targets:
                            if target in fp_lower:
                                code = clean_view_content(content)
                                if code:
                                    is_tr = "<truncated" in content or "does not show" in content.lower()
                                    length = len(code)
                                    best = best_dbt[target]
                                    if (best["content"] is None) or (best["truncated"] and not is_tr) or (best["truncated"] == is_tr and length > best["len"]):
                                        best_dbt[target] = {
                                            "len": length,
                                            "content": code,
                                            "conv": conv,
                                            "step": sidx or idx,
                                            "truncated": is_tr
                                        }
    except Exception as e:
        pass

for target, info in best_dbt.items():
    print(f"Target: {target} | Len: {info['len']} | Truncated: {info['truncated']} | Conv: {info['conv']} | Step: {info['step']}")
    if info["content"]:
        dest = os.path.join(workspace_dir, "dbt", target.replace("/", "\\"))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8") as f:
            f.write(info["content"])
        print(f"Wrote to {dest}")
