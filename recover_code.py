import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
conversations = [
    "43501d54-94a1-43ac-8dd7-7b9742dce801",
    "cf3c8394-0a7b-4e70-acf5-b3bbbb227053",
    "b8be5ad8-2723-4f7f-a057-27cef6eac548"
]

recovered_files = {}

# Regex to match line numbers like "123: content"
line_num_pattern = re.compile(r"^(\s*\d+):\s(.*)$")

for conv in conversations:
    log_path = os.path.join(brains_dir, conv, ".system_generated", "logs", "transcript.jsonl")
    if not os.path.exists(log_path):
        print(f"Log path {log_path} does not exist.")
        continue
    
    print(f"Reading log: {log_path}...")
    
    # We will read line by line. We can keep track of tool calls and their matching responses.
    # To do this, we can store tool calls by step index or match them with the subsequent response.
    steps = []
    with open(log_path, 'r', encoding='utf-8') as f:
        for line in f:
            try:
                steps.append(json.loads(line))
            except Exception as e:
                pass

    for i, step in enumerate(steps):
        stype = step.get("type")
        sindex = step.get("step_index")
        
        # 1. Check for write_to_file tool calls
        if "tool_calls" in step:
            for tc in step["tool_calls"]:
                tname = tc.get("name")
                args = tc.get("arguments", tc.get("args", {}))
                if isinstance(args, str):
                    try:
                        args = json.loads(args)
                    except:
                        pass
                if isinstance(args, dict):
                    if tname == "write_to_file":
                        target_file = args.get("TargetFile")
                        code_content = args.get("CodeContent")
                        if target_file and code_content:
                            target_file = target_file.strip('\'" \t\n\r')
                            # Normalize path
                            norm_path = target_file.lower().replace("\\", "/")
                            recovered_files[norm_path] = (target_file, code_content, f"write_to_file in step {sindex}")
        
        # 2. Check for VIEW_FILE outputs
        if stype == "VIEW_FILE":
            content = step.get("content", "")
            # Find the path from content or from the preceding step's tool call
            path_match = re.search(r"File Path:\s*`file:///(.*?)`", content)
            file_path = None
            if path_match:
                file_path = path_match.group(1).replace("%20", " ")
            else:
                # Fallback: look at the previous step's tool call
                prev_step = steps[i-1] if i > 0 else None
                if prev_step and "tool_calls" in prev_step:
                    for tc in prev_step["tool_calls"]:
                        if tc.get("name") == "view_file":
                            args = tc.get("arguments", tc.get("args", {}))
                            if isinstance(args, str):
                                try: args = json.loads(args)
                                except: pass
                            if isinstance(args, dict):
                                file_path = args.get("AbsolutePath")
            
            if file_path:
                file_path = file_path.strip('\'" \t\n\r')
                norm_path = file_path.lower().replace("\\", "/")
                # Parse lines to strip line numbers
                lines = content.splitlines()
                code_lines = []
                started = False
                for line in lines:
                    if "The following code has been modified to include a line number" in line:
                        started = True
                        continue
                    if not started:
                        continue
                    # Match line number pattern
                    m = line_num_pattern.match(line)
                    if m:
                        code_lines.append(m.group(2))
                    elif line.strip() == "The above content shows the entire, complete file contents of the requested file.":
                        break
                    elif started:
                        # Sometimes lines might not match if they are part of a multiline block or format?
                        # Usually they match. If not, just append the line
                        code_lines.append(line)
                
                if code_lines:
                    code_content = "\n".join(code_lines)
                    # Only overwrite if we don't have a newer write or if it's longer
                    if norm_path not in recovered_files or len(code_content) > len(recovered_files[norm_path][1]):
                        recovered_files[norm_path] = (file_path, code_content, f"view_file in step {sindex}")

# Print what we found
print(f"\nFound {len(recovered_files)} files in logs:")
for norm_path, (orig_path, content, source) in recovered_files.items():
    print(f"- {orig_path} (length: {len(content)}, source: {source})")
    
# Let's create a recovery folder and save them!
recovery_dir = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios\recovered"
os.makedirs(recovery_dir, exist_ok=True)
for norm_path, (orig_path, content, source) in recovered_files.items():
    # If the file belongs to pro-tes, we can write it back to its relative path under bemi_bios!
    if "pro-tes/" in norm_path or "/pro-tes/" in norm_path:
        # Determine relative path from pro-tes/
        parts = norm_path.split("/pro-tes/")
        rel_path = parts[-1]
        dest_path = os.path.join(r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios", rel_path.replace("/", "\\"))
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        with open(dest_path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Recovered and wrote: {dest_path}")
    else:
        # Save to recovery_dir with its basename
        base = os.path.basename(orig_path)
        dest_path = os.path.join(recovery_dir, base)
        with open(dest_path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Recovered (other): {dest_path}")
