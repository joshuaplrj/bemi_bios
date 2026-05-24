import os
import json
import re

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"

# Target files we want to reconstruct
targets = {
    "dxe/bemibioscore.c": "bemibioscore.c",
    "post/postroutines.c": "postroutines.c",
    "cpuid/cpuidspoof.c": "cpuidspoof.c",
    "msr/msrshadow.c": "msrshadow.c",
    "smm/smmhandler.c": "smmhandler.c",
    "common/hypervisorbackend.c": "hypervisorbackend.c",
    "svm/svmcore.c": "svmcore.c",
    "vmx/vmxcore.c": "vmxcore.c",
    "bwgov/bandwidthgovernor.c": "bandwidthgovernor.c",
    "bwgov/memorycompressor.c": "memorycompressor.c",
    "fusion/macroopfusion.c": "macroopfusion.c",
    "interrupt/interruptlatency.c": "interruptlatency.c",
    "l0cache/l0microcache.c": "l0microcache.c",
    "rob/robdistributor.c": "robdistributor.c",
    "tage/tagepredictor.c": "tagepredictor.c",
    "tests/qemutest.py": "qemutest.py",
    "tests/testsuite.c": "testsuite.c",
    "dbt/ir.rs": "ir.rs",
    "dbt/bin/sim8088.rs": "sim8088.rs",
    "dbt/codegen/mod.rs": "codegen_mod.rs",
    "dbt/executor/mod.rs": "executor_mod.rs"
}

# Collect all edits
# Format: {target_key: [list of edits]}
# Each edit: {"type": "write"|"replace"|"multi", "content": ..., "target": ..., "replacement": ..., "chunks": ..., "conv": ..., "step": ...}
all_edits = {k: [] for k in targets}

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
                
                sidx = step.get("step_index")
                tool_calls = step.get("tool_calls", [])
                
                for tc in tool_calls:
                    tname = tc.get("name")
                    if tname not in ["write_to_file", "replace_file_content", "multi_replace_file_content"]:
                        continue
                        
                    args = tc.get("arguments", tc.get("args", {}))
                    if isinstance(args, str):
                        try: args = json.loads(args)
                        except: pass
                    if not isinstance(args, dict):
                        continue
                        
                    tf_path = args.get("TargetFile") or ""
                    if not tf_path:
                        continue
                        
                    norm_path = tf_path.lower().replace("\\", "/").replace("//", "/")
                    
                    # Find which target this matches
                    matched_key = None
                    for tk in targets:
                        if tk in norm_path:
                            matched_key = tk
                            break
                            
                    if not matched_key:
                        continue
                        
                    if tname == "write_to_file":
                        code = args.get("CodeContent") or ""
                        all_edits[matched_key].append({
                            "type": "write",
                            "content": code,
                            "conv": conv,
                            "step": sidx or idx,
                            "truncated": "<truncated" in code
                        })
                    elif tname == "replace_file_content":
                        target_content = args.get("TargetContent") or ""
                        repl_content = args.get("ReplacementContent") or ""
                        start = args.get("StartLine")
                        end = args.get("EndLine")
                        all_edits[matched_key].append({
                            "type": "replace",
                            "target": target_content,
                            "replacement": repl_content,
                            "start": start,
                            "end": end,
                            "conv": conv,
                            "step": sidx or idx,
                            "truncated": "<truncated" in repl_content
                        })
                    elif tname == "multi_replace_file_content":
                        chunks = args.get("ReplacementChunks") or []
                        all_edits[matched_key].append({
                            "type": "multi",
                            "chunks": chunks,
                            "conv": conv,
                            "step": sidx or idx
                        })
    except Exception as e:
        print(f"Error reading conv {conv}: {e}")

# Process edits for each target
# We sort edits chronologically, but wait: we have multiple conversations.
# Let's sort them. How to sort across conversations?
# We can look at the creation timestamps in the logs!
# Let's read timestamps for each step.
# For simplicity, let's print how many edits we found for each file first.
for tk, edits in all_edits.items():
    print(f"File: {tk} -> Found {len(edits)} edits. Any non-truncated write? {any(e['type'] == 'write' and not e['truncated'] for e in edits)}")
    for e in edits:
        if e['type'] == 'write':
            print(f"  Write: Conv {e['conv']} Step {e['step']} Len {len(e['content'])} Truncated {e['truncated']}")
        elif e['type'] == 'replace':
            print(f"  Replace: Conv {e['conv']} Step {e['step']} Truncated {e['truncated']}")
        elif e['type'] == 'multi':
            print(f"  Multi: Conv {e['conv']} Step {e['step']} Chunks {len(e['chunks'])}")
