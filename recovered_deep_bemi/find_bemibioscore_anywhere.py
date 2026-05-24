import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
path_pattern = re.compile(r"File Path:\s*`file:///(.*?)`", re.IGNORECASE)

if not os.path.exists(brains_dir):
    print("Brains dir not found")
    exit(1)

found_instances = []

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
                if "bemibioscore.c" in line.lower():
                    # Parse json
                    try:
                        step = json.loads(line)
                    except Exception:
                        continue
                    
                    sidx = step.get("step_index")
                    stype = step.get("type")
                    content = step.get("content", "")
                    
                    # Check write_to_file in tool_calls
                    tool_calls = step.get("tool_calls", [])
                    for tc in tool_calls:
                        if tc.get("name") in ["write_to_file", "replace_file_content", "multi_replace_file_content"]:
                            args
<truncated 2224 bytes>
 Truncated: {inst['truncated']}")

# If we found any non-truncated instances, let's write the longest one to BemiBiosCore.c!
non_truncated = [inst for inst in found_instances if not inst['truncated'] and inst['length'] > 100]
if non_truncated:
    # Sort by length descending
    non_truncated.sort(key=lambda x: x['length'], reverse=True)
    best = non_truncated[0]
    print(f"\nBest non-truncated instance found in Conv: {best['conv']}, Step: {best['step']}, Type: {best['type']} (length={best['length']})")
    
    # Let's clean the content if it's VIEW_FILE (strip line numbers)
    raw_content = best['content']
    if best['type'] == "VIEW_FILE":
        lines = raw_content.splitlines()
        code_lines = []
        started = False
        line_num_pattern = re.compile(r"^(\s*\d+):\s(.*)$")
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
        cleaned_content = "\n".join(code_lines)
    else:
        cleaned_content = raw_content
        
    dest_path = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios\bemibiospkg\bemibioscore\dxe\bemibioscore.c"
    with open(dest_path, 'w', encoding='utf-8') as out:
        out.write(cleaned_content)
    print(f"Successfully wrote best recovered content to {dest_path}")
else:
    print("\nNo non-truncated instances found.")
