import os

workspace = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"
for root, dirs, files in os.walk(workspace):
    # skip recovered, __pycache__, .git
    if any(p in root for p in ["recovered", "__pycache__", ".git"]):
        continue
    for file in files:
        if file.endswith((".c", ".h", ".inf", ".dec", ".dsc")):
            path = os.path.join(root, file)
            try:
                with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                    for line_num, line in enumerate(f, 1):
                        if "BemiBiosEntryPoint" in line:
                            print(f"{path}:{line_num}: {line.strip()}")
            except Exception as e:
                pass
