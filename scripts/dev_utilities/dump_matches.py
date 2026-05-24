import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"

matches = [
    ("43501d54-94a1-43ac-8dd7-7b9742dce801", 22),
    ("43501d54-94a1-43ac-8dd7-7b9742dce801", 113),
    ("43501d54-94a1-43ac-8dd7-7b9742dce801", 723),
    ("b8be5ad8-2723-4f7f-a057-27cef6eac548", 20),
    ("ce14097b-5bff-4a33-87e9-c3d8b2c235a3", 69),
    ("ce14097b-5bff-4a33-87e9-c3d8b2c235a3", 70),
    ("cf3c8394-0a7b-4e70-acf5-b3bbbb227053", 430),
    ("cf3c8394-0a7b-4e70-acf5-b3bbbb227053", 480),
    ("cf3c8394-0a7b-4e70-acf5-b3bbbb227053", 521),
    ("cf3c8394-0a7b-4e70-acf5-b3bbbb227053", 649)
]

for conv, sidx in matches:
    log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
    with open(log_path, 'r', encoding='utf-8') as f:
        for line in f:
            try:
                step = json.loads(line)
            except:
                continue
            if step.get("step_index") == sidx:
                print(f"=== {conv} Step {sidx} ===")
                print(f"Type: {step.get('type')}")
                content = step.get("content", "")
                if content:
                    print(f"Content (first 300 chars): {content[:300]}...")
                tool_calls = step.get("tool_calls", [])
                for tc in tool_calls:
                    print(f"Tool call: {tc.get('name')}")
                    args = tc.get("arguments", tc.get("args", {}))
                    if isinstance(args, str):
                        try: args = json.loads(args)
                        except: pass
                    if isinstance(args, dict):
                        print(f"  TargetFile: {args.get('TargetFile') or args.get('AbsolutePath')}")
                        code = args.get('CodeContent') or args.get('ReplacementContent') or ""
                        print(f"  Code len: {len(code)}, Truncated: {'<truncated' in code}")
                print()
