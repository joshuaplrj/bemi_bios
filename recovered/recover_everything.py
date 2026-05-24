import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
workspace_dir = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"

if not os.path.exists(brains_dir):
    print("Brains dir not found")
    exit(1)

# We will collect the best version of each file path.
# Key: normalized path (lower case, forward slashes, relative or absolute)
# Value: (original_path, content, length, source_info, is_truncated)
recovered_files = {}

line_num_pattern = re.compile(r"^(\s*\d+):\s(.*)$")

def clean_view_content(content):
    lines = content.splitlines()
    code_lines = []
    started = False
    for line in lines:
            started = True
            continue
        if not started:
            continue
        m = line_num_pattern.match(line)
        if m:
            code_lines.append(m.group(2))
        elif line.strip() == "The above content shows the entire, complete file contents of the requested file.":
            break
        elif started:
            code_lines.append(line)
    
    if code_lines:
        return "\n".join(code_lines)
    return None

for conv in os.listdir(brains_dir):
    conv_path = os.path.join(brains_dir, conv)
    if not os.path.isdir(conv_path):
        continue
    log_path = os.path.join(conv_path, ".system_generated", "logs", 
<truncated 2627 bytes>
 " ")
                        norm_path = file_path.lower().replace("\\", "/")
                        code = clean_view_content(content)
                        if code:
                            is_tr = "<truncated" in content or "<truncated" in code
                            if norm_path not in recovered_files:
                                recovered_files[norm_path] = (file_path, code, len(code), f"view_file in {conv}:{sidx}", is_tr)
                            else:
                                old_tf, old_code, old_len, old_src, old_tr = recovered_files[norm_path]
                                if (old_tr and not is_tr) or (old_tr == is_tr and len(code) > old_len):
                                    recovered_files[norm_path] = (file_path, code, len(code), f"view_file in {conv}:{sidx}", is_tr)
    except Exception as e:
        print(f"Error reading conv {conv}: {e}")

print(f"\nCollected {len(recovered_files)} unique files from logs.")

# Now, filter and write relevant files to the workspace!
# Relevant files are those that belong under bemi_bios (specifically those with pro-tes/ in the path)
for norm_path, (orig_path, content, length, source, is_tr) in recovered_files.items():
    if "pro-tes/" in norm_path or "/pro-tes/" in norm_path:
        # Determine the relative path under pro-tes/
        parts = norm_path.split("/pro-tes/")
        rel_path = parts[-1].replace("/", "\\")
        dest_path = os.path.join(workspace_dir, rel_path)
        
        # Make sure directory exists
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        
        # Write to destination
        with open(dest_path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Recovered & Wrote: {rel_path} (len={length}, truncated={is_tr}, source={source})")
