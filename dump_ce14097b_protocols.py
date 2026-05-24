import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "ce14097b-5bff-4a33-87e9-c3d8b2c235a3"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

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
    return "\n".join(code_lines)

with open(log_path, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            step = json.loads(line)
        except Exception:
            continue
        sidx = step.get("step_index")
        content = step.get("content", "")
        if sidx == 33:
            code = clean_view_content(content)
            dest = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios\bemibiospkg\bemibioscore\protocol\bemiprotocol.c"
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, 'w', encoding='utf-8') as out:
                out.write(code)
            print(f"Wrote BemiProtocol.c (len={len(code)})")
        elif sidx == 34:
            code = clean_view_content(content)
            dest = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios\bemibiospkg\bemibioscore\protocol\bemiprotocol.h"
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, 'w', encoding='utf-8') as out:
                out.write(code)
            print(f"Wrote BemiProtocol.h (len={len(code)})")
