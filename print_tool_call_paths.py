import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"

found_paths = set()

for conv in os.listdir(brains_dir):
    conv_path = os.path.join(brains_dir, conv)
    if not os.path.isdir(conv_path):
        continue
    log_path = os.path.join(conv_path, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        continue
        
    try:
        with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
            for idx, line in enumerate(f):
                try:
                    step = json.loads(line)
                except Exception:
                    continue
                
                tool_calls = step.get("tool_calls", [])
                for tc in tool_calls:
                    tname = tc.get("name")
                    if tname not in ["write_to_file", "replace_file_content", "multi_replace_file_content"]:
                        continue
                    args = tc.get("arguments", tc.get("args", {}))
                    if isinstance(args, str):
                        try: args = json.loads(args)
                        except: pass
                    if isinstance(args, dict):
                        tf_path = args.get("TargetFile")
                        if tf_path:
                            found_paths.add(tf_path.lower().replace("\\", "/"))
    except Exception as e:
        pass

print(f"Total paths found in tool calls: {len(found_paths)}")
for p in sorted(list(found_paths)):
    if any(ext in p for ext in [".c", ".h", ".rs", ".py", ".nasm"]):
        print(" ", p)
