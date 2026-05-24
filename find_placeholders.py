import os

folders = ["bemi_book", "docs", "res-docs"]
placeholders = ["TODO", "FIXME", "TBD", "placeholder", "[ ]"]

print("Starting placeholder and link audit...")
found_count = 0

for folder in folders:
    folder_path = os.path.join(os.path.dirname(__file__), folder)
    if not os.path.exists(folder_path):
        continue
    for root, dirs, files in os.walk(folder_path):
        for file in files:
            if not file.endswith(".md"):
                continue
            file_path = os.path.join(root, file)
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
            for idx, line in enumerate(lines):
                # Check for placeholders
                for pl in placeholders:
                    if pl in line:
                        # Exclude harmless examples or markdown checkboxes like [x]
                        if pl == "[ ]" and ("[ ]" in line or "TODO.md" in line):
                            # wait, [ ] is a checkbox. Let's see if it's an uncompleted task.
                            print(f"[UNCOMPLETED TASK] {file}:{idx+1}: {line.strip()}")
                            found_count += 1
                        elif pl != "[ ]":
                            # Ignore TODO.md reference
                            if "TODO.md" in line and pl == "TODO":
                                continue
                            print(f"[PLACEHOLDER] {file}:{idx+1} ({pl}): {line.strip()}")
                            found_count += 1
                # Check for broken or empty links
                if "]()" in line or "](file:///)" in line or "file:////" in line:
                    print(f"[BROKEN LINK] {file}:{idx+1}: {line.strip()}")
                    found_count += 1

print(f"Audit completed. Found {found_count} issues.")
