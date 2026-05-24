import os

search_paths = [
    r"C:\Users\John Jacob\Desktop",
    r"C:\Users\John Jacob\.gemini"
]

target_names = ["postroutines.c", "hypervisorbackend.c", "vmxcore.c", "svmcore.c"]

found = []
for base_path in search_paths:
    if not os.path.exists(base_path):
        continue
    for root, dirs, files in os.walk(base_path):
        # skip python cache and large generated folders to avoid slowdown
        if any(p in root for p in ["__pycache__", ".git", "conversations"]):
            continue
        for file in files:
            if file.lower() in target_names:
                path = os.path.join(root, file)
                found.append((path, os.path.getsize(path)))

print(f"Found {len(found)} instances of target files globally:")
for path, size in found:
    print(f"  {path} (size={size})")
