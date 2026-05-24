import os

workspace_dir = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"
for root, dirs, files in os.walk(workspace_dir):
    for file in files:
        if file.endswith(".md"):
            path = os.path.join(root, file)
            try:
                with open(path, "r", encoding="utf-8") as f:
                    content = f.read()
                    if "unfinished" in content.lower():
                        print(f"File: {os.path.relpath(path, workspace_dir)}")
            except:
                pass
