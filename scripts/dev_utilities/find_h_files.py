import os

workspace = r"c:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"
for root, dirs, files in os.walk(workspace):
    for file in files:
        if file.lower().endswith(".h"):
            print(os.path.join(root, file))
