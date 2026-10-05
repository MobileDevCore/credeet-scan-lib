# ScanMatch C — Production Multilingual Product Matching Engine

ScanMatch C is an ultra-lightweight, high-performance C engine (`libscanmatch.so` on Linux, `libscanmatch.dll` on Windows) integrated with a Python FastAPI REST API. It specializes in Gujarati-first, Hindi, and English shopping-list normalization, conservative quantity/unit extraction, and catalog matching.

---

## 1. System Architecture

```text
Image or Text Input
      ↓
FastAPI (/api/v1/scan, /api/v1/match)
      ↓
Python OCR Interface (server/ocr_service.py)
      ↓
MockOCR (Local, offline, deterministic)
      ↓
OCRResult(text, confidence, ...)
      ↓
Python ctypes FFI (python/scanmatch_ctypes.py)
      ↓
libscanmatch.so (Production C Engine)
      ├─ Gujarati / Devanagari numeral normalization (૦-૯, ०-९ → 0-9)
      ├─ Unit parsing (કિલો, કિ.ગ્રા., લિટર, નંગ, ડઝન, પેકેટ, etc.)
      ├─ Strict Quantity & Unit extraction (never invents missing values)
      ├─ Catalog matching (Exact, Token overlap, Levenshtein edit distance)
      ├─ Calibrated confidence scoring & candidate ranking
      └─ Structured JSON output
      ↓
FastAPI JSON Response
```

### Important Architectural Guarantees:
- **MockOCR is Local & Offline:** Fully deterministic, runs 100% offline with zero external network calls, zero cloud dependencies, and zero Google credentials.
- **Future OCR Provider Separation:** Google Cloud Vision is designed as a future replaceable Python OCR provider implementing `BaseOCR`. It is **not** part of the C library.
- **C Library Decoupling:** `libscanmatch` contains zero OCR or AI dependencies. It compiles cleanly with C11 and links only the standard C math library (`-lm`).
- **No Duplicate Logic:** No OCR code exists in C; no catalog matching logic is duplicated in Python.

---

## 2. Directory Structure

```text
ScanMatch/
├── CMakeLists.txt              # Standard C11 CMake build configuration
├── README.md                   # System documentation and operational guide
├── run_server.py               # Uvicorn server launcher
│
├── src/                        # Production C engine source
│   ├── scanmatch.c             # Catalog matching & Unicode Levenshtein distance
│   ├── parser.c                # Multilingual quantity & unit parser
│   ├── text.c                  # Gujarati/Hindi/English normalization & UTF-8 utils
│   └── ffi.c                   # Reentrant C context & JSON serialization FFI
│
├── include/                    # Public C headers
│   ├── scanmatch.h             # Core data structures and matching API
│   ├── scanmatch_ffi.h         # Context lifecycle and JSON FFI exports
│   └── scanmatch_text.h        # Text normalization and UTF-8 declarations
│
├── cli/                        # CLI utility for text processing & troubleshooting
│   └── scanmatch_cli.c
│
├── python/                     # Python ctypes FFI layer
│   ├── __init__.py
│   └── scanmatch_ctypes.py     # ScanMatch FFI wrapper with safe memory cleanup
│
├── server/                     # FastAPI application & OCR service
│   ├── app.py                  # Endpoints: /health, /api/v1/match, /api/v1/scan
│   ├── ocr_service.py          # BaseOCR, MockOCR (local/offline), CloudVision stub
│   └── requirements.txt        # FastAPI, Uvicorn, Python-Multipart
│
├── data/                       # Production kirana catalog
│   └── products.csv            # 56 items across 10 kirana categories
│
├── tests/                      # Automated regression & integration test suites
│   ├── test_similarity.c       # Unicode string similarity tests
│   ├── test_parser.c           # Quantity & unit parsing tests
│   ├── test_ffi.c              # C FFI context & JSON tests
│   ├── test_matching.c         # Product matching tests
│   ├── test_gujarati.c         # Gujarati digit and script normalization tests
│   ├── test_catalog_gujarati.c # Real catalog matching tests
│   ├── test_comprehensive.c    # End-to-end multi-language test suite
│   └── test_api_integration.py # 16 FastAPI + MockOCR + FFI test scenarios
│
├── tools/                      # Development & troubleshooting tools
│   ├── test_server_api.py      # HTTP endpoint verification
│   └── test_edge_cases.py      # Multilingual edge case validation
│
└── build/                      # Build outputs
    └── libscanmatch.so / .dll  # Production shared library
```

---

## 3. Product Catalog (`data/products.csv`)

The catalog contains 56 kirana products across 10 categories:
- **Grains & Staples:** Rice (ચોખા / चावल), Wheat (ઘઉં / गेहूं), Atta, Maida, Besan, Poha, Rava, Corn Flour, Rolled Flakes
- **Pulses:** Tuver Dal, Moong Dal, Chana Dal, Masoor Dal, Urad Dal, Soyabean
- **Sweeteners:** Sugar (ખાંડ / चीनी), Jaggery / Gud (ગોળ / गुड़)
- **Spices & Salts:** Salt / Namak (મીઠું / नमक), Haldi, Mirchi, Jeera, Dhania, Rai, Ajwain, Black Pepper
- **Dry Fruits:** Almonds (બદામ / बादाम), Cashews / Kaju (કાજુ / काजू), Raisins, Pistachio
- **Snacks & Biscuits:** Papad, Mamra, Sev, Parle-G, Biscuits
- **Personal Care:** Soap, Lux Soap, Shampoo, Clinic Plus, Toothpaste, Toothbrush, Dant Manjan
- **Household:** Detergent, Dishwash, Agarbatti, Matchbox, Dhoop
- **Edible & Hair Oils:** Sing Tel (Groundnut Oil), Sunflower Oil, Mustard Oil, Dabur Amla Hair Oil
- **Dairy & Beverages:** Milk (દૂધ / दूध), Tea (ચા / चाय), Coffee

Each product entry provides canonical names, brand, category, aliases in English, Gujarati (`aliases_gu`), Hindi (`aliases_hi`), common OCR typos, and standard units/pack sizes.

---

## 4. Build Instructions

### Linux / DigitalOcean (Target Deployment)
```bash
# Standard CMake build generates genuine ELF 64-bit libscanmatch.so
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Output: `build/libscanmatch.so` (genuine Linux ELF shared library).

### Windows / MSYS2 (Local Development)
```bash
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
```

Output: `build/libscanmatch.dll`.

---

## 5. Running the REST API Server

```bash
# Install dependencies
pip install -r server/requirements.txt

# Start server locally
python run_server.py
```

### Endpoints
- `GET /health` — Health check, catalog status, and active OCR engine (`mock`).
- `POST /api/v1/match` — JSON matching endpoint (`{"ocr_text": "..."}`).
- `POST /api/v1/scan` — Multipart image upload endpoint (processed via MockOCR locally).

---

## 6. Running Automated Tests

```bash
# 1. C Test Suite (7 unit & comprehensive test suites)
ctest --test-dir build --output-on-failure

# 2. FastAPI End-to-End Integration Tests (16 scenarios)
python tests/test_api_integration.py

# 3. HTTP Server API Endpoint Verification
python tools/test_server_api.py

# 4. Multilingual Edge Case Validation
python tools/test_edge_cases.py
```

---

## 7. Confidence & Status Policy

- **Confirmed (`status = "confirmed"`):** High confidence (score ≥ 0.80), unambiguous match. `needs_confirmation = false`.
- **Suggested (`status = "suggested"`):** Moderate confidence (0.35 ≤ score < 0.80). `needs_confirmation = true`.
- **Ambiguous (`status = "ambiguous"`):** Multiple candidates within 0.08 score delta. `needs_confirmation = true`.
- **Unidentified (`status = "unidentified"`):** Score < 0.35. `product_id = null`, `confidence = 0.0`.
- **Conservative Quantities:** Never invents quantity or unit if omitted from the input line.
