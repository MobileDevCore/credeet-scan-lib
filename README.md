# ScanMatch C — Production-Hardened Multilingual Product Matching Engine

ScanMatch C is an ultra-lightweight, high-performance C shared library (`libscanmatch.so` / `libscanmatch.dll`) and Python FastAPI integration designed for local and cloud deployment. It specializes in Gujarati-first, Hindi, and English shopping-list normalization, conservative quantity/unit extraction, and catalog matching.

## Architecture

```text
User image / text
    ↓
Python FastAPI (server/app.py) [Concurrent, lock-free matching]
    ↓
OCR Layer (server/ocr_service.py) [MockOCR offline / Cloud Vision when configured]
    ↓  (Extracted text)
Python FFI (python/scanmatch_ctypes.py)
    ↓  (In-memory string pointer)
libscanmatch: sm_process_text_json
    ├─ 1. Gujarati / Devanagari numeral normalization (૦-૯, ०-९ → 0-9)
    ├─ 2. Suffix modifier & unit parsing (pukat → packet, theli → bag, walu suffix)
    ├─ 3. Strict Quantity & Unit extraction (never invents missing values)
    ├─ 4. Exact-First Matching (canonical, alias, token overlap before fuzzy)
    ├─ 5. Bounded Stack-Allocated Levenshtein (zero heap allocations in common loop)
    ├─ 6. Calibrated Confidence Scoring & Candidate Preservation
    └─ 7. Explainable JSON serialization with fallback product text
    ↓
FastAPI returns structured JSON response to client
```

## Key Principles & Guarantees

1. **Clean Separation of Concerns:**
   - Image OCR is decoupled in Python (`server/ocr_service.py`).
   - `libscanmatch` focuses purely on text normalization, quantity/unit parsing, and catalog matching.
   - Zero cloud credentials or heavy OCR runtimes inside the C shared library.
2. **Minimal & Optimized Footprint:**
   - Memory strategy: Bounded 256-element stack array for Unicode Levenshtein dynamic programming rows, eliminating heap `malloc`/`free` calls per word comparison.
   - Concurrency: Read-only catalog matching enables concurrent multi-threaded requests in FastAPI without lock serialization.
   - Early length pruning skips O(N×M) edit distance when length delta precludes high similarity.
3. **Optimistic-Yet-Honest Matching:**
   - **High Confidence (>= 0.80):** `status = "confirmed"`, `needs_confirmation = false`.
   - **Medium Confidence (0.50–0.79):** `status = "suggested"` or `"ambiguous"`, `needs_confirmation = true`.
   - **Low Confidence (0.35–0.49):** Preserves best candidate as `status = "suggested"`, `needs_confirmation = true`, retaining actual score.
   - **Unidentified (< 0.35):** `status = "unidentified"`, `product_id = null`, `confidence = 0.0`.
   - **Conservative Quantities/Units:** Never invents a quantity or unit if absent (`has_quantity = false`, `quantity = null`).
   - **Product Text Fallback:** Cleaned `product_text` and `normalized_text` are always preserved.
4. **Gujarati-First Language Features:**
   - Gujarati Unicode digits (`૦`, `૧`, `૨`, `૩`, `૪`, `૫`, `૬`, `૭`, `૮`, `૯`) normalized to standard ASCII (`0`-`9`).
   - Common Gujarati OCR confusion recovery (`૫` misclassified as `પ` before units).
   - Gujarati shopping units (`કિલો`, `કિ.ગ્રા.`, `લિટર`, `નંગ`, `ડઝન`, `પેકેટ`, `થેલી`, etc.) alongside Hindi and English equivalents.
   - Colloquial modifier handling (`વાળુ`, `વાળું`, `વાળા`, `walu` stripped gracefully).
   - Multi-byte UTF-8 codepoint edit similarity.

## Product Catalog (`data/products.csv`)

The catalog covers 37 common Indian kirana products across 10 major categories:
- **Staples:** Rice, Wheat, Atta / Wheat Flour, Maida, Besan, Poha / Pava, Rava / Suji, Corn Flour
- **Pulses:** Tuver / Toor Dal, Moong Dal, Chana Dal, Masoor Dal, Urad Dal, Soyabean
- **Sweeteners:** Sugar, Jaggery / Gud / Gol
- **Spices & Salts:** Salt / Namak, Turmeric / Haldi, Red Chilli, Cumin / Jeera, Coriander / Dhania, Mustard / Rai
- **Dry Fruits:** Almonds, Cashews / Kaju, Raisins / Kishmish, Pistachio / Pista
- **Snacks:** Papad, Mamra / Puffed Rice, Sev, Parle-G Biscuit, Biscuits
- **Household:** Soap, Lux Soap, Detergent, Dishwash, Agarbatti, Matchbox
- **Personal Care:** Shampoo, Clinic Plus Shampoo, Toothpaste, Toothbrush
- **Oils:** Groundnut Oil / Sing Tel, Sunflower Oil, Mustard Oil
- **Dairy & Beverages:** Milk, Tea, Coffee

Each item contains English, Gujarati (`aliases_gu`), Hindi (`aliases_hi`), and common OCR error aliases.

## Build from Source

### On Windows / MSYS2
```bash
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
```

### On Linux / DigitalOcean
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Running the FastAPI REST Server

```bash
# Install server requirements
pip install -r server/requirements.txt

# Run server
python run_server.py
```

### API Endpoints
- `GET /health` — Check server status, catalog status, and active OCR backend.
- `POST /api/v1/match` — Ingest raw OCR text directly (JSON body: `{"ocr_text": "..."}`).
- `POST /api/v1/scan` — Upload shopping-list image file or form text.

## Running Tests

```bash
# 1. C Unit & Comprehensive Tests
ctest --test-dir build --output-on-failure

# 2. FastAPI Integration Tests (16 test scenarios)
python tests/test_api_integration.py

# 3. Server API Endpoint Tests
python tools/test_server_api.py

# 4. Multilingual Kirana Edge Case Tests
python tools/test_edge_cases.py
```

## Known Limitations

1. **OCR Backend:** Google Cloud Vision is not configured by default; local mock OCR is used unless credentials are provided in the environment.
2. **Catalog Scope:** Matching is bounded by the loaded catalog (`data/products.csv`). Products outside the catalog with similarity < 0.35 are marked `unidentified`.
3. **Handwriting Legibility:** Extremely corrupted or illegible text strings where word boundaries cannot be determined will return `unidentified`.
