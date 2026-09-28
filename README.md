# ScanMatch C — v10 Gujarati-first improvement baseline

This is the **existing ScanMatch v10 main project**, updated in-place with a Gujarati-first shopping-list recognition path. The architecture is now frozen; future work should focus on measured OCR quality, Gujarati data, catalog quality, regression coverage, calibration and performance.

## Frozen architecture
```text
Image
 ↓
Validation
 ↓
Preprocessing variants + orientation
 ↓
Gujarati-first OCR / multilingual fallback
 ↓
Gujarati + multilingual normalization
 ↓
Item segmentation
 ↓
Product / quantity / unit parser
 ↓
Catalog + alias + OCR-error + fuzzy matching
 ↓
Confidence / confirmation
 ↓
Structured JSON
 ↓
API / CLI
```

## Gujarati-first improvements in this v10 ZIP
- Gujarati Unicode normalization and script detection.
- Gujarati and Devanagari numeral normalization, including multi-digit and decimal quantities.
- Recovery for the common Gujarati OCR confusion `૫ → પ` when it occurs in numeric position before a unit.
- Gujarati shopping units: `કિલો`, `કિલોગ્રામ`, `ગ્રામ`, `લિટર`, `લીટર`, `મિલીલીટર`, `નંગ`, `પીસ`, `ડઝન`, `પેક`, `બોટલ` plus English/Hindi equivalents.
- Product/quantity/unit separation before catalog matching.
- Gujarati aliases and common OCR variants in the catalog.
- Unicode code-point fuzzy similarity instead of byte-by-byte Gujarati comparison.
- Gujarati-first Tesseract pass followed by the requested multilingual fallback.
- Multiple preprocessing variants, scaling, border cropping and four-way orientation trials.
- Raw OCR and normalized text retained in the API response.
- Structured candidates, confidence and confirmation state.
- Catalog is loaded once per server process and protected by a request lock around the shared C context.
- Gujarati regression tests are part of CTest.

## Build from a clean checkout
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The project uses the Tesseract C API and Leptonica. CMake checks for both development packages and emits a direct installation diagnostic when either is missing.

## CLI
```bash
build/scanmatch_cli shopping_list.jpg
```
Optional catalog/language arguments:
```bash
build/scanmatch_cli shopping_list.jpg data/products.csv guj+eng+hin
```

## API
Install Python dependencies from `server/requirements.txt`, build the C library, then:
```bash
python run_server.py
```
Endpoints:
- `POST /api/v1/scan`
- `POST /scan` compatibility endpoint
- `GET /health`

## Gujarati examples
The parser and normalization layer cover examples such as:
```text
ચોખા ૫ કિલો
ચોખા પ કિલો
ખાંડ ૫૦૦ ગ્રામ
દૂધ ૧ લિટર
સાબુ ૪ નંગ
ચા ૫૦૦ g
ચોખા 1.5 કિલો
```

The API separates these into product, quantity and canonical unit. The original OCR line remains available as `raw_text`.

## Accuracy policy
The source code does **not** claim 95% recognition accuracy. The acceptance targets are engineering goals and must be measured on a representative labeled Gujarati dataset. Use `tools/evaluate.py` and `tools/benchmark.py` to measure product accuracy, quantity/unit accuracy, top-3 recall, false acceptance and latency.

Handwritten Gujarati accuracy depends strongly on the OCR model and the handwriting/photo dataset. Tesseract remains a baseline until a dedicated handwriting model is trained and validated.

## Business rules
- Quantity and unit never become part of the catalog identity.
- Duplicate list entries are preserved; aggregation is an application-level decision.
- High-confidence results can be auto-accepted; uncertain results expose candidates and require confirmation.
- Generic products such as `soap` are not silently converted to a brand when the catalog does not provide sufficient evidence.
- Customer shopping lists are transient input. Persistent learning belongs to the shop catalog and approved aliases/OCR variants, not customer history.
