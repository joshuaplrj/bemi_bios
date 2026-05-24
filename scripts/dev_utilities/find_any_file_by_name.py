import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conv = "cf3c8394-0a7b-4e70-acf5-b3bbbb227053"
log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")

if not os.path.exists(log_path):
    print("Log not found")
    exit(1)

with open(log_path, 'r', encoding='utf-8') as f:
    for idx, line in enumerate(f):
        if "BemiBiosCore.c" in line:
            try:
                step = json.loads(line)
            except Exception:
                continue
            
            content = step.get("content", "")
            sidx = step.get("step_index")
            # If the step is a tool call, search there
            tool_calls = step.get("tool_calls", [])
            for tc in tool_calls:
                args = tc.get("arguments", tc.get("args", {}))
                if isinstance(args, str):
                    try: args = json.loads(args)
                    except: pass
                if isinstance(args, dict):
                    tf = args.get("TargetFile") or args.get("AbsolutePath")
                    if tf and "BemiBiosCore.c" in tf:
                        print(f"Step {sidx or idx}: tool_call {tc.get('name')} target={tf}")
            
            if "File Path:" in content and "BemiBiosCore.c" in content:
                print(f"Step {sidx or idx}: VIEW_FILE content for BemiBiosCore.c (len={len(content)})")
                if "<truncated" in content:
                    print("  [Truncated]")
                else:
                    print("  [NOT Truncated]")
