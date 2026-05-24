import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "cf3c8394-0a7b-4e70-acf5-b3bbbb227053"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

if not os.path.exists(log_path):
    print(f"Error: {log_path} does not exist.")
    exit(1)

print(f"Reading {log_path}...")
line_num_pattern = re.compile(r"^(\s*\d+):\s(.*)$")

with open(log_path, 'r', encoding='utf-8') as f:
    for idx, line in enumerate(f):
        try:
            step = json.loads(line)
        except Exception as e:
            continue
        
        # We can look for write_to_file or view_file tool calls or content matching BemiBiosCore.c
        content = step.get("content", "")
        # Also check tool_calls
        tool_calls = step.get("tool_calls", [])
        
        # Check if the content is view_file of BemiBiosCore.c
        if "BemiBiosCore.c" in content and "File Path:" in content:
            # Let's count lines and see if it's truncated
            if "<truncated" not in content:
                # Extract the code
                lines = content.splitlines()
                code_lines = []
                started = False
                for l in lines:
                        started = True
                        continue
                    if not started:
                        continue
                    m = line_num_pattern.match(l)
                    if m:
                        code_lines.append(m.group(2))
                    elif l.strip() == "The above content shows the entire, complete file contents of the requested file.":
                        break
                    elif started:
                        code_lines.append(l)
                
                if code_lines:
                    full_code = "\n".join(code_lines)
                    print(f"Found non-truncated version in step_index={step.get('step_index')} (length={len(full_code)} bytes)")
                    # Save it!
                    dest_path = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios\bemibiospkg\bemibioscore\dxe\bemibioscore.c"
                    with open(dest_path, 'w', encoding='utf-8') as out:
                        out.write(full_code)
                    print(f"Successfully wrote recovered file to {dest_path}")
                    exit(0)

print("Could not find non-truncated BemiBiosCore.c in that log.")
