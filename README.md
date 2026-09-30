# ScanMatch C — Minimal, Optimized Multilingual Product Matching Engine

ScanMatch C is an ultra-lightweight (58 KB), high-performance C shared library (`libscanmatch.so`) and Python FastAPI integration designed for deployment on Linux / DigitalOcean. It specializes in Gujarati-first, Hindi, and English shopping-list normalization, conservative quantity/unit extraction, and catalog fuzzy matching.

## Target Architecture

### Current Flow (Active in Phase 6 — 100% Offline):
```text
User image
    ↓
Python FastAPI (server/app.py)
    ↓
Mock/Local OCR (server/ocr_service.py) [Deterministic, zero network calls]
    ↓  (Extracted text & confidence)
Python FFI (python/scanmatch_ctypes.py)
    ↓  (In-memory string pointer)
libscanmatch: sm_process_text_json
    ├─ 1. Gujarati / Devanagari numeral normalization (૦-૯, ०-९ → 0-9)
    ├─ 2. Multi-lingual script detection & Unicode normalization
    ├─ 3. Strict Quantity & Unit extraction (no invented values)
    ├─ 4. Product catalog & alias hybrid matching (exact, token, Levenshtein)
    ├─ 5. Confidence scoring & candidate ambiguity detection
    └─ 6. Explainable JSON serialization
    ↓
FastAPI returns structured JSON response to client
```

### Future Flow (Cloud Vision Replacement):
```text
FastAPI → Google Cloud Vision (server/ocr_service.py) → Python FFI → libscanmatch → JSON
```

> **IMPORTANT NOTICE:**
> - **Google Cloud Vision is NOT configured yet.**
> - **No Google account, credentials, API keys, or billing are required for local/mock testing.**
> - The application operates 100% offline with zero cloud dependencies.

## Key Principles & Guarantees

1. **Clean Separation of Concerns:**
   - Google Cloud Vision handles the image OCR in Python.
   - `libscanmatch.so` is dedicated purely to text normalization, quantity/unit parsing, and catalog matching.
   - Zero HTTP, cloud credentials, or external heavy OCR runtimes (Tesseract/PaddleOCR) inside `libscanmatch.so`.
2. **Minimal & Optimized Footprint:**
   - Library size: **58 KB**
   - Runtime dependencies: Standard C11 library (`libc`) and math (`libm`). No third-party C libraries.
   - Latency: **<2 ms** per shopping list batch.
3. **Conservative & Reassuring Behavior:**
   - **Never invents quantity:** If absent, `has_quantity = false` and `quantity = null`.
   - **Never invents unit:** If absent, `has_unit = false` and `unit = null`.
   - **Never invents products:** Low confidence results are marked `unidentified` with `confidence = 0.0`.
   - **Ambiguity Detection:** If multiple products have close top scores, the item is explicitly flagged as `ambiguous` with `needs_confirmation = true`.
   - **No OCR Text:** Empty or whitespace-only inputs return a clean `NO_TEXT_DETECTED` result.
4. **Gujarati-First Language Features:**
   - Gujarati Unicode digits (`૦`, `૧`, `૨`, `૩`, `૪`, `૫`, `૬`, `૭`, `૮`, `૯`) normalized to standard ASCII (`0`-`9`).
   - Common Gujarati OCR confusion recovery (`૫` misclassified as `પ` before units).
   - Gujarati shopping units (`કિલો`, `કિ.ગ્રા.`, `લિટર`, `નંગ`, `ડઝન`, `પેકેટ`, etc.) alongside Hindi and English equivalents.
   - Multi-byte UTF-8 character-level Levenshtein similarity.

## Build from Source

### On Linux / DigitalOcean (Target Environment)
Produces a genuine ELF 64-bit shared library (`build/libscanmatch.so`):
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# Verify ELF binary
file build/libscanmatch.so
ldd build/libscanmatch.so
nm -D build/libscanmatch.so
```

### On Windows / MSYS2 (Local Development Environment)
Produces a Windows PE32+ DLL (`build/libscanmatch.dll`):
```bash
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure

# Verify Windows DLL
file build/libscanmatch.dll
```

## Running the FastAPI REST Server

```bash
# Install server requirements
pip install -r server/requirements.txt

# Run server
python run_server.py
```

The server automatically starts in safe local **mock OCR mode** if no cloud credentials are provided, allowing full end-to-end testing without external network calls or cloud costs.

### API Endpoints

- `GET /health` — Check server status, loaded catalog, and active OCR backend (`mock` or `google_cloud_vision`).
- `POST /api/v1/match` — Ingest raw OCR text directly (JSON body: `{"ocr_text": "..."}`).
- `POST /api/v1/scan` — Upload shopping-list image file (multipart/form-data).

## Running Tests

```bash
# C unit tests (similarity, parser, FFI, matching, Gujarati, catalog, comprehensive)
ctest --test-dir build --output-on-failure

# Python FastAPI integration tests
python tests/test_api_integration.py

# CLI test
build/scanmatch_cli "ચોખા ૨ કિલો"
```
