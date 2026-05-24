import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "cf3c8394-0a7b-4e70-acf5-b3bbbb227053"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

if not os.path.exists(log_path):
    print(f"Error: {log_path} does not exist.")
    exit(1)

with open(log_path, 'r', encoding='utf-8') as f:
    for idx, line in enumerate(f):
        try:
            step = json.loads(line)
        except Exception as e:
            continue
        
        content = step.get("content", "")
        # Check if the content has "BemiBiosCore.c" and "File Path:"
        if "BemiBiosCore.c" in content and "File Path:" in content:
            # print some details
            print(f"Step {step.get('step_index')}:")
            print(f"  Length of content: {len(content)}")
            # print first 5 lines of the file content in it
            lines = content.splitlines()
            count = 0
            for l in lines:
                if "The following code" in l or count > 0:
                    print(f"    {l}")
                    count += 1
                    if count > 8:
                        break
