import os

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
if os.path.exists(brains_dir):
    print(f"Dirs in {brains_dir}:")
    for name in os.listdir(brains_dir):
        path = os.path.join(brains_dir, name)
        if os.path.isdir(path):
            print(f"  {name}")
else:
    print(f"{brains_dir} does not exist.")
