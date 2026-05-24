import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
targets = ["sim8088.rs", "ir.rs", "executor/mod.rs", "codegen/mod.rs", "x8088.rs", "decoder.rs", "translator.rs"]

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

best_versions = {t: (0, "", "") for t in targets} # target: (len, code, conv_step_details)

for conv in os.listdir(brains_dir):
    conv_path = os.path.join(brains_dir, conv)
    if not os.path.isdir(conv_path):
        continue
    log_path = os.path.join(conv_path, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
    try:
        with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
4
<truncated 868 bytes>
             code = args.get("CodeContent") or args.get("ReplacementContent") or ""
                            if tf and code:
                                for target in targets:
                                    if target in tf.lower().replace("\\", "/"):
                                        if "<truncated" not in code and len(code) > best_versions[target][0]:
                                            best_versions[target] = (len(code), code, f"Tool {tname} in Conv {conv} step {step.get('step_index') or idx}")
                
                # Check view file content
                if stype == "VIEW_FILE" and "File Path:" in content:
                    path_match = re.search(r"File Path:\s*`file:///(.*?)`", content)
                    if path_match:
                        file_path = path_match.group(1).replace("%20", " ")
                        code = clean_view_content(content)
                        if code and "<truncated" not in code:
                            for target in targets:
                                if target in file_path.lower().replace("\\", "/"):
                                    if len(code) > best_versions[target][0]:
                                        best_versions[target] = (len(code), code, f"View in Conv {conv} step {step.get('step_index') or idx}")
    except Exception as e:
        pass

for target, (length, code, details) in best_versions.items():
    print(f"Target: {target} | Best Len: {length} | Source: {details}")
    if length > 0:
        # Write to a recovered file in scratch so we can examine it
        dest = os.path.join(brains_dir, "43501d54-94a1-43ac-8dd7-7b9742dce801", "scratch", f"recovered_{target.replace('/', '_')}")
        with open(dest, "w", encoding="utf-8") as f:
            f.write(code)
        print(f"  Wrote best version to {dest}")
