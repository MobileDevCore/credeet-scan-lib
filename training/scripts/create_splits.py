#!/usr/bin/env python3
"""
ScanMatch C — Train / Validation / Test Dataset Splitter
Strictly separates line samples to prevent training data leakage.
Rule: Do not train and test on the same images.
"""
import sys, shutil, random
from pathlib import Path

def create_splits(source_dir: Path, output_base: Path, train_ratio=0.70, val_ratio=0.15):
    print("=== DATASET PARTITIONING (STRICT SEPARATION) ===")
    print("RULE: Do not train and test on the same images.\n")

    train_dir = output_base / "train"
    val_dir = output_base / "validation"
    test_dir = output_base / "test"

    for d in [train_dir, val_dir, test_dir]:
        d.mkdir(parents=True, exist_ok=True)

    gt_files = sorted(list(source_dir.glob("*.gt.txt")))
    if not gt_files:
        print(f"No .gt.txt files found in {source_dir}")
        return 1

    # Shuffle deterministically
    random.seed(42)
    random.shuffle(gt_files)

    n_total = len(gt_files)
    n_train = int(n_total * train_ratio)
    n_val = int(n_total * val_ratio)
    n_test = n_total - (n_train + n_val)

    splits = {
        "train": (gt_files[:n_train], train_dir),
        "validation": (gt_files[n_train:n_train + n_val], val_dir),
        "test": (gt_files[n_train + n_val:], test_dir),
    }

    for split_name, (files, dest_dir) in splits.items():
        print(f"Creating {split_name} split ({len(files)} items)...")
        for gt_path in files:
            base = gt_path.name[:-7]
            shutil.copy2(gt_path, dest_dir / gt_path.name)
            # Find and copy matching image
            for img_path in source_dir.glob(f"{base}.*"):
                if img_path.suffix.lower() in [".png", ".tif", ".tiff", ".jpg", ".jpeg"]:
                    shutil.copy2(img_path, dest_dir / img_path.name)

    print("\nPartitioning complete:")
    print(f"   Train samples:      {n_train} ({train_ratio*100:.0f}%) -> {train_dir}")
    print(f"   Validation samples: {n_val} ({val_ratio*100:.0f}%) -> {val_dir}")
    print(f"   Test samples:       {n_test} ({(1-train_ratio-val_ratio)*100:.0f}%) -> {test_dir}")
    return 0

if __name__ == "__main__":
    src = Path("training/data/ground_truth")
    base = Path("training/data")
    sys.exit(create_splits(src, base))
