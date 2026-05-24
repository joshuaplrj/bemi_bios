import os

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
target = "acpitables.c"

for root, dirs, files in os.walk(brains_dir):
    for file in files:
        if file.endswith(".jsonl") or file.endswith(".log") or file.endswith(".txt"):
            path = os.path.join(root, file)
            try:
                with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                    for idx, line in enumerate(f):
                        if target in line.lower():
                            # print relpath and snippet of line
                            rel = os.path.relpath(path, brains_dir)
                            print(f"{rel}:{idx+1} -> {line.strip()[:150]}")
            except Exception as e:
                pass
