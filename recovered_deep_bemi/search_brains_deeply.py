import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
workspace_dir = r"C:\Users\John Jacob\Desktop\extras\test-box\vemi\bemi_bios"

target_files = [
    "bemibioscore.c", "postroutines.c", "cpuidspoof.c", "msrshadow.c",
    "smmhandler.c", "hypervisorbackend.c", "svmcore.c", "vmxcore.c",
    "bandwidthgovernor.c", "memorycompressor.c", "macroopfusion.c",
    "interruptlatency.c", "l0microcache.c", "robdistributor.c",
    "tagepredictor.c", "qemutest.py", "testsuite.c", "ir.rs", "lib.rs",
    "bemiprotocol.c", "bemiprotocol.h", "postasm.nasm", "vmxexitasm.nasm",
    "svmasm.nasm", "svmexitasm.nasm"
]

# We want to find the best version for each file
best_versions = {f: {"len": 0, "content": None, "conv": None, "step": None, "truncated": True} for f in target_files}

# Line pattern to strip line numbers from view_file
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
        elif line.strip() == "The above content shows the entire, complete file contents of the requested fil
<truncated 3590 bytes>
                basename = os.path.basename(file_path).lower()
                        if basename in best_versions:
                            code = clean_view_content(content)
                            if code:
                                is_tr = "<truncated" in content or "<truncated" in code
                                length = len(code)
                                best = best_versions[basename]
                                if (best["content"] is None) or (best["truncated"] and not is_tr) or (best["truncated"] == is_tr and length > best["len"]):
                                    best_versions[basename] = {
                                        "len": length,
                                        "content": code,
                                        "conv": conv,
                                        "step": sidx or idx,
                                        "truncated": is_tr
                                    }
    except Exception as e:
        print(f"Error reading conv {conv}: {e}")

print("\nBest versions found:")
for filename, info in best_versions.items():
    print(f"File: {filename} | Len: {info['len']} | Truncated: {info['truncated']} | Conv: {info['conv']} | Step: {info['step']}")

# Write the best versions we found (even if truncated, it's better than nothing, but let's see which ones we can find)
# We will write them to the recovered directory first so we don't mess up the workspace yet.
recovered_dir = os.path.join(workspace_dir, "recovered_deep")
os.makedirs(recovered_dir, exist_ok=True)
for filename, info in best_versions.items():
    if info["content"]:
        with open(os.path.join(recovered_dir, filename), "w", encoding="utf-8") as f:
            f.write(info["content"])
        print(f"Wrote to recovered_deep/{filename}")
