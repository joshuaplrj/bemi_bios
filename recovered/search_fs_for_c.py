import os

search_root = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi"
found = []

for root, dirs, files in os.walk(search_root):
    if "__pycache__" in root or ".git" in root:
        continue
    for file in files:
        if file.endswith((".c", ".h", ".txt", ".bak", ".tmp")):
            path = os.path.join(root, file)
            try:
                with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    if "gBemiProtocol" in content:
                        found.append((path, len(content)))
            except Exception:
                pass

print(f"Found {len(found)} files matching search criteria:")
for path, length in found:
    print(f"  {path} (length={length})")
