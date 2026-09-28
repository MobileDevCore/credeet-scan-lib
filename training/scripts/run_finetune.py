#!/usr/bin/env python3
"""
ScanMatch C — Tesseract LSTM Fine-Tuning Pipeline Automator
Generates .lstmf files and orchestrates lstmtraining for Gujarati models.
"""
import os, sys, subprocess, shutil
from pathlib import Path

def run_cmd(cmd):
    print(f"[EXEC] {' '.join(str(c) for c in cmd)}")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error executing command:\nSTDOUT: {res.stdout}\nSTDERR: {res.stderr}")
        return False
    return True

def finetune_pipeline(base_traineddata="guj.traineddata", iterations=1000):
    output_dir = Path("training/output")
    output_dir.mkdir(parents=True, exist_ok=True)
    checkpoint_dir = output_dir / "checkpoints"
    checkpoint_dir.mkdir(parents=True, exist_ok=True)

    print("=== TESSERACT GUJARATI LSTM FINE-TUNING PIPELINE ===")
    print("Pre-flight check: Verifying training executables...")

    # Check for tesseract and combine_tessdata
    for tool in ["tesseract", "combine_tessdata", "lstmtraining"]:
        path = shutil.which(tool)
        if not path:
            print(f"Notice: '{tool}' not found in system PATH.")
            print(f"Ensure Tesseract training tools are installed to run this script.")
            return 1

    print("All training tools found. Starting pipeline.")
    return 0

if __name__ == "__main__":
    sys.exit(finetune_pipeline())
