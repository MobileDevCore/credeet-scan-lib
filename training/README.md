# ScanMatch C — Gujarati Tesseract Fine-Tuning Workflow

This directory contains the data layout, tooling, and pipeline documentation for fine-tuning Tesseract's LSTM neural network on real and handwritten Gujarati shopping slips.

---

## 1. Overview & Architecture

```text
Real Gujarati Shopping Slips & Handwritten Receipts
                      ↓
           Raw Document Acquisition (`data/raw/`)
                      ↓
  Line Segmentation & Ground-Truth Labeling (`data/ground_truth/`)
                      ↓
           Train / Val / Test Partitioning (`create_splits.py`)
                      ↓
          Feature Extraction to `.lstmf` via Tesseract
                      ↓
 LSTM Fine-Tuning (`lstmtraining` from `guj.traineddata` base model)
                      ↓
   Trained Model Packaging (`combine_tessdata` → `output/guj_shopping.traineddata`)
                      ↓
     Evaluation on Unseen Test Split (CER, WER, SKU Accuracy)
                      ↓
    ScanMatch C Runtime Integration (via `SCANMATCH_TESSDATA`)
```

> [!WARNING]
> **CRITICAL DATA HYGIENE RULE**:
> **Do not train and test on the same images.**
> All evaluations (Character Error Rate, Word Error Rate, and SKU accuracy) must be conducted exclusively on the held-out `data/test/` partition to ensure generalization to real-world store receipts.

---

## 2. Directory Structure

```text
training/
├── README.md                           # This guide
├── data/
│   ├── raw/                            # High-resolution uncropped scans and smartphone receipts
│   ├── ground_truth/                   # Line image (.png/.tif) + text (.gt.txt) pairs
│   │   ├── sample_01.png
│   │   ├── sample_01.gt.txt            # "ચોખા ૫ કિલો"
│   │   ├── sample_02.png
│   │   ├── sample_02.gt.txt            # "દૂધ ૨ લિટર"
│   │   ├── sample_03.png
│   │   └── sample_03.gt.txt            # "સાબુ ૪ નંગ"
│   ├── train/                          # 70% split for LSTM training
│   ├── validation/                     # 15% split for early stopping / validation
│   └── test/                           # 15% unseen test split for acceptance benchmark
├── scripts/
│   ├── prepare_data.py                 # Validates line crops, Unicode normalization, and gt.txt pairs
│   ├── create_splits.py                # Deterministic train/validation/test split generator
│   ├── run_finetune.py                 # Automates lstmtraining pipeline execution
│   └── evaluate_model.py               # Evaluates CER, WER, and product matching accuracy
└── output/                             # Destination for checkpoints and final .traineddata
```

---

## 3. Required Tools & Prerequisites

* **Tesseract Version**: Tesseract 5.3.0+ or 5.5.x with training tools compiled (`tesstrain`, `lstmtraining`, `combine_tessdata`, `unicharset_extractor`).
* **Base Model**: Official `guj.traineddata` from `tesseract-ocr/tessdata_best` (the full float precision LSTM model, not the fast integerized model).
* **Python Dependencies**: Python 3.8+, Pillow, Levenshtein (optional).

On Windows (MSYS2 UCRT64):
```bash
pacman -S mingw-w64-ucrt-x86_64-tesseract-ocr mingw-w64-ucrt-x86_64-tesseract-data-guj
```

---

## 4. Dataset Guidelines

### Line-Level Ground Truth Format
Each sample consists of:
1. `sample_NNN.png` (or `.tif`): Single cropped line of text, height scaled to approx 48–64 pixels, grayscale or binary.
2. `sample_NNN.gt.txt`: Single line of UTF-8 encoded text exactly matching the image, terminated with a newline.

### Gujarati Character & Numeral Coverage
Ensure the dataset comprehensively covers:
* **Gujarati Numerals**: `૦` (0), `૧` (1), `૨` (2), `૩` (3), `૪` (4), `૫` (5), `૬` (6), `૭` (7), `૮` (8), `૯` (9).
* **Key Unit Words**:
  * `કિલો`, `કિલોગ્રામ`, `કિ.ગ્રા.`
  * `ગ્રામ`, `ગ્રા.`
  * `લિટર`, `લીટર`
  * `મિલીલીટર`, `મિલી`
  * `નંગ`, `પીસ`, `ડઝન`, `પેકેટ`, `બોટલ`, `બોક્સ`
* **Common Handwriting Confusions**:
  * The letter `પ` (Pa) vs numeral `૫` (5)
  * Matra `િ` (short i) vs `ી` (long ee)
  * Anusvara dots `ં` on `ખાંડ`, `નંગ`, `ઘઉં`
  * Joint ligatures (જોડાક્ષર) such as `ક્લ`, `ક્સ` (e.g. `ક્લિનિક`, `લક્સ`)
* **Bilingual / Mixed Samples**:
  * `ચોખા 5 kg`
  * `Soap 4 pcs`
  * `Clinic Plus 2 bottle`

---

## 5. Fine-Tuning Execution Pipeline

### Step 1: Validate Dataset
```bash
python training/scripts/prepare_data.py --data_dir training/data/ground_truth
```

### Step 2: Generate Splits
```bash
python training/scripts/create_splits.py --ratio 70:15:15
```

### Step 3: Extract Base LSTM Model
```bash
combine_tessdata -e guj.traineddata training/output/guj.lstm
```

### Step 4: Generate .lstmf Feature Files
For each line image:
```bash
tesseract sample.png sample --psm 7 -l guj lstm.train
```

### Step 5: Run LSTM Fine-Tuning
```bash
lstmtraining \
  --continue_from training/output/guj.lstm \
  --traineddata guj.traineddata \
  --train_listfile training/output/train_files.txt \
  --eval_listfile training/output/val_files.txt \
  --model_output training/output/checkpoints/guj_shopping \
  --max_iterations 5000 \
  --target_error_rate 0.01
```

### Step 6: Combine Checkpoint into Production .traineddata
```bash
lstmtraining --stop_training \
  --continue_from training/output/checkpoints/guj_shopping_checkpoint \
  --traineddata guj.traineddata \
  --model_output training/output/guj_shopping.traineddata
```

---

## 6. Model Evaluation Protocol

Run evaluation exclusively against the unseen test split:
```bash
python training/scripts/evaluate_model.py \
  --model training/output/guj_shopping.traineddata \
  --test_dir training/data/test
```

Key Metrics Measured:
1. **Character Error Rate (CER)**: Percentage of character insertions, deletions, and substitutions. Target: `< 3%` on printed, `< 8%` on clean handwriting.
2. **Word Error Rate (WER)**: Target: `< 5%` on shopping list vocabulary.
3. **Product & Quantity Parsing Accuracy**: End-to-end extraction accuracy in ScanMatch C. Target: `≥ 95%`.

---

## 7. Using the Fine-Tuned Model in ScanMatch C

ScanMatch C is designed to load custom trained models without modifying C source code:

### Option A: Via Environment Variable (Recommended)
Place `guj_shopping.traineddata` in `training/output/guj.traineddata` and run:
```powershell
$env:SCANMATCH_TESSDATA = "C:\Users\DEEP\Desktop\Scanmatch\training\output"
build/scanmatch_cli.exe images/shopping_list.png data/products.csv guj
```

### Option B: Via Standard Tesseract Data Directory
Copy `guj_shopping.traineddata` to your Tesseract tessdata directory as `guj.traineddata`.
ScanMatch will automatically use the updated weights.
