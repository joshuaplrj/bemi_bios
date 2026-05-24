import os

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "cf3c8394-0a7b-4e70-acf5-b3bbbb227053"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

if not os.path.exists(log_path):
    print("Log not found")
    exit(1)

with open(log_path, 'r', encoding='utf-8') as f:
    for idx, line in enumerate(f):
        if "bemibioscore.c" in line.lower():
            # Print index, length and a small snippet
            print(f"Line {idx}: length {len(line)}")
            # Try to see if there's a tool_call or stype
            # print first 200 chars
            print(f"  Start: {line[:200]}")
