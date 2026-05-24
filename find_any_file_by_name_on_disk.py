import os

search_root = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"
for root, dirs, files in os.walk(search_root):
    for file in files:
        if file.lower().endswith((".c", ".h", ".nasm")):
            # Print path relative to workspace
            rel = os.path.relpath(os.path.join(root, file), search_root)
            print(rel)
