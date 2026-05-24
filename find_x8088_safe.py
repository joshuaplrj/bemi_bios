import os
import json

brains_dir = r"C:\Users\John Jacob\.gemini\antigravity\brain"

with open("x8088_matches.txt", "w", encoding="utf-8") as f_out:
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
                    if "x8088" in line.lower() or "x8088executor" in line.lower():
                        try:
                            step = json.loads(line)
                        except:
                            continue
                        sidx = step.get("step_index")
                        stype = step.get("type")
                        tcs = step.get("tool_calls", [])
                        tc_names = [tc.get("name") for tc in tcs]
                        
                        f_out.write(f"Conv: {conv} | Line: {idx} | Step: {sidx} | Type: {stype} | Tools: {tc_names}\n")
        except Exception as e:
            f_out.write(f"Error reading conv {conv}: {e}\n")

print("Scan complete. Results written to x8088_matches.txt.")
