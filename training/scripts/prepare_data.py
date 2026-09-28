#!/usr/bin/env python3
"""
ScanMatch C — Dataset Validation Tool
Validates image and .gt.txt pairs for Tesseract LSTM fine-tuning.
"""
import sys
from pathlib import Path
from PIL import Image

def validate_dataset(data_dir: Path):
    print(f"Validating dataset in: {data_dir.resolve()}")
    if not data_dir.exists():
        print(f"Error: Directory {data_dir} does not exist.")
        return 1

    gt_files = list(data_dir.glob("*.gt.txt"))
    if not gt_files:
        print(f"Warning: No .gt.txt files found in {data_dir}.")
        return 0

    valid_count = 0
    errors = 0

    for gt_path in sorted(gt_files):
        base = gt_path.name[:-7]
        # Check image counterpart
        img_candidates = list(data_dir.glob(f"{base}.*"))
        img_candidates = [p for p in img_candidates if p.suffix.lower() in [".png", ".tif", ".tiff", ".jpg", ".jpeg"]]

        if not img_candidates:
            print(f"Error: Missing image for ground truth text: {gt_path.name}")
            errors += 1
            continue

        img_path = img_candidates[0]

        # Validate text content
        try:
            text = gt_path.read_text(encoding="utf-8").strip()
            if not text:
                print(f"Error: Empty ground-truth text in {gt_path.name}")
                errors += 1
                continue
        except UnicodeDecodeError:
            print(f"Error: Non-UTF8 encoding in {gt_path.name}")
            errors += 1
            continue

        # Validate image geometry
        try:
            with Image.open(img_path) as img:
                w, h = img.size
                if w < 10 or h < 10:
                    print(f"Error: Image too small ({w}x{h}) in {img_path.name}")
                    errors += 1
                    continue
        except Exception as e:
            print(f"Error reading image {img_path.name}: {e}")
            errors += 1
            continue

        valid_count += 1

    print(f"\nValidation Summary:")
    print(f"   Valid line pairs: {valid_count}")
    print(f"   Errors detected:  {errors}")
    return 0 if errors == 0 else 1

if __name__ == "__main__":
    target = Path("training/data/ground_truth") if len(sys.argv) < 2 else Path(sys.argv[1])
    sys.exit(validate_dataset(target))
