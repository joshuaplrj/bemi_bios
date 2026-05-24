import os
import glob

def scan_docs():
    folders = [
        "c:/Users/John Jacob/Desktop/extras/test-box/vemi/bemi_bios/bemi_book",
        "c:/Users/John Jacob/Desktop/extras/test-box/vemi/bemi_bios/docs",
        "c:/Users/John Jacob/Desktop/extras/test-box/vemi/docs"
    ]
    
    keywords = ["TODO", "FIXME", "placeholder", "tbd", "to be determined", "insert here", "draft", "[ ]"]
    
    found_any = False
    
    for folder in folders:
        if not os.path.exists(folder):
            print(f"Directory {folder} does not exist!")
            continue
        print(f"\nScanning {folder}...")
        files = glob.glob(os.path.join(folder, "*.md"))
        for file_path in files:
            try:
                with open(file_path, "r", encoding="utf-8") as f:
                    content = f.read()
                
                # Check for keywords
                lower_content = content.lower()
                for kw in keywords:
                    if kw.lower() in lower_content:
                        # Find occurrences
                        lines = content.splitlines()
                        for i, line in enumerate(lines):
                            if kw.lower() in line.lower():
                                print(f"  [Found '{kw}'] {os.path.basename(file_path)}:L{i+1}: {line.strip()[:100]}")
                                found_any = True
            except Exception as e:
                print(f"  Error reading {file_path}: {e}")
                
    if not found_any:
        print("\nNo placeholders found!")
    else:
        print("\nDone scanning. Placeholders found.")

if __name__ == "__main__":
    scan_docs()
