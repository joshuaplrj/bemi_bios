import os

search_root = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"
for root, dirs, files in os.walk(search_root):
    # skip ignore paths
    if any(p in root for p in ["__pycache__", ".git", "recovered"]):
        continue
    for file in files:
        if file.lower().endswith((".c", ".h", ".nasm", ".py")):
            path = os.path.join(root, file)
            try:
                with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    if "<truncated" in content:
                        print(f"TRUNCATED: {os.path.relpath(path, search_root)} (len={len(content)})")
            except Exception:
                pass
