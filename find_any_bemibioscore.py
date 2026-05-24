import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conversations = [
    "43501d54-94a1-43ac-8dd7-7b9742dce801",
    "cf3c8394-0a7b-4e70-acf5-b3bbbb227053",
    "b8be5ad8-2723-4f7f-a057-27cef6eac548"
]

line_num_pattern = re.compile(r"^(\s*\d+):\s(.*)$")

for conv in conversations:
    log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
    
    print(f"\n--- Checking conversation {conv} ---")
    with open(log_path, 'r', encoding='utf-8') as f:
        for idx, line in enumerate(f):
            try:
                step = json.loads(line)
            except Exception as e:
                continue
            
            content = step.get("content", "")
            # We can search for keywords unique to BemiBiosCore.c such as:
            # "BemiBiosEntryPoint" or "gBemiProtocol" or "PostDetectTopology"
            if "BemiBiosEntryPoint" in content and "PostDetectTopology" in content:
                print(f"Step {step.get('step_index') or idx}: content length: {len(content)}")
                # Check if it has the truncated tag
                if "<truncated" in content:
                    print("  [Truncated]")
                else:
                    print("  [Not Truncated!]")
                    # Show first 15 lines of the code block
                    lines = content.splitlines()
                    code_started = False
                    print_count = 0
                    for l in lines:
                        if "include <Uefi.h>" in l or code_started:
                            code_started = True
                            print(f"    {l}")
                            print_count += 1
                            if print_count > 15:
                                break
