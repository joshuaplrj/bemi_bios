import os

workspace_dir = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"
check_dirs = ["bemibiospkg", "hwcompat", "hypervisor", "performance", "tests", "dbt", "deploy"]

print("Analyzing active files:")
for root, dirs, files in os.walk(workspace_dir):
    # Skip recovered or target folders
    if any(d in root for d in ["recovered", "recovered_deep", ".git", "target", "__pycache__"]):
        continue
    for file in files:
        if file.lower().endswith((".c", ".h", ".rs", ".py")):
            path = os.path.join(root, file)
            try:
                with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                if "<truncated" in content:
                    # Find the line number and surrounding text
                    lines = content.splitlines()
                    for idx, line in enumerate(lines):
                        if "<truncated" in line:
                            rel = os.path.relpath(path, workspace_dir)
                            print(f"File: {rel} | Line: {idx+1} | Content: {line.strip()} | Total lines: {len(lines)}")
            except Exception as e:
                print(f"Error reading {file}: {e}")
