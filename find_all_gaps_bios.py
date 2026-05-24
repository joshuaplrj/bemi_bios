import os

workspace = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"
extensions = (".c", ".h", ".rs", ".nasm")

gaps = {}

for root, dirs, files in os.walk(workspace):
    if any(p in root for p in ["__pycache__", ".git", "target", "recovered", "recovered_deep", "recovered_deep_bemi"]):
        continue
    for file in files:
        if file.endswith(extensions):
            path = os.path.join(root, file)
            rel = os.path.relpath(path, workspace)
            with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            
            file_gaps = []
            for idx, line in enumerate(lines):
                if "missing line" in line.lower():
                    file_gaps.append(idx + 1)
            
            if file_gaps:
                gaps[rel] = file_gaps

print(f"Found {len(gaps)} files with gaps:")
for rel, lines in sorted(gaps.items()):
    # group contiguous lines
    ranges = []
    if not lines:
        continue
    start = lines[0]
    prev = lines[0]
    for l in lines[1:]:
        if l == prev + 1:
            prev = l
        else:
            if start == prev:
                ranges.append(f"{start}")
            else:
                ranges.append(f"{start}-{prev}")
            start = l
            prev = l
    if start == prev:
        ranges.append(f"{start}")
    else:
        ranges.append(f"{start}-{prev}")
    
    print(f"  {rel}: lines {', '.join(ranges)} (total {len(lines)} lines)")
