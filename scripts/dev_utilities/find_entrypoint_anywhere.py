import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"
if not os.path.exists(brains_dir):
    print("Brains dir not found")
    exit(1)

found = []
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
                if "BemiBiosEntryPoint" in line:
                    try:
                        step = json.loads(line)
                    except Exception:
                        continue
                    
                    content = step.get("content", "")
                    # check if this contains C code
                    if "Uefi.h" in content or "EFI_SYSTEM_TABLE" in content:
                        found.append({
                            "conv": conv,
                            "step": step.get("step_index") or idx,
                            "length": len(content),
                            "truncated": "<truncated" in content,
                            "content": content
                        })
    except Exception as e:
        pass

print(f"Found {len(found)} C-like matches for BemiBiosEntryPoint:")
for i, inst in enumerate(found):
    print(f"  {i}: Conv: {inst['conv']} | Step: {inst['step']} | Length: {inst['length']} | Truncated: {inst['truncated']}")

if found:
    # Sort by length descending and non-truncated first
    found.sort(key=lambda x: (not x['truncated'], x['length']), reverse=True)
    best = found[0]
    print(f"\nBest match: Conv: {best['conv']} | Step: {best['step']} | Length: {best['length']} | Truncated: {best['truncated']}")
    # print snippet of content around BemiBiosEntryPoint
    idx_ep = best['content'].find("BemiBiosEntryPoint")
    print("Snippet:")
    print(best['content'][max(0, idx_ep-200):min(len(best['content']), idx_ep+1000)])
