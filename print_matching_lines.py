import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conversations = [
    "43501d54-94a1-43ac-8dd7-7b9742dce801",
    "cf3c8394-0a7b-4e70-acf5-b3bbbb227053",
    "b8be5ad8-2723-4f7f-a057-27cef6eac548"
]

for conv in conversations:
    log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
    
    print(f"\n--- Searching {conv} ---")
    with open(log_path, 'r', encoding='utf-8') as f:
        for idx, line in enumerate(f):
            # Look for indicators of the file content
            if "BemiBiosEntryPoint" in line or "gBemiProtocol" in line or "BemiGetBootMode" in line:
                try:
                    step = json.loads(line)
                except Exception:
                    continue
                sidx = step.get("step_index")
                stype = step.get("type")
                # Look at the tool calls in the step
                tool_calls = step.get("tool_calls", [])
                tc_names = [tc.get("name") for tc in tool_calls] if tool_calls else []
                print(f"Step {sidx or idx} | Type: {stype} | ToolCalls: {tc_names} | Line length: {len(line)}")
                # Print a small snippet of the line around the match
                start_idx = max(0, line.find("BemiBiosEntryPoint") - 50)
                end_idx = min(len(line), line.find("BemiBiosEntryPoint") + 150)
                print(f"  Snippet: ...{line[start_idx:end_idx]}...")
