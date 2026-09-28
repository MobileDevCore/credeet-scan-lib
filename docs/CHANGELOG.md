# Changelog

## v10 Gujarati-first update
Applied directly to the existing `scanmatch_c_v10.zip` main project.

### Recognition
- Added Gujarati-first OCR selection when `guj` is requested.
- Added four-way orientation trials.
- Added border crop, scaling, grayscale, adaptive threshold and fixed-threshold preprocessing variants.
- Added basic image-quality scoring and usable/retake guidance.

### Gujarati understanding
- Added Unicode-aware normalization.
- Added Gujarati and Hindi numeral normalization, including multi-digit and decimal values.
- Added a targeted `૫ → પ` numeric OCR recovery rule when parsing quantities.
- Added Gujarati shopping units and canonical unit normalization.
- Added Gujarati product aliases and common OCR variants.
- Replaced byte-oriented fuzzy comparison with UTF-8 code-point comparison.

### Shopping-list behavior
- Product, quantity and unit are parsed separately.
- Multiple lines are independently processed.
- API returns raw text, normalized text, parsed fields, candidates, confidence and confirmation state.
- Catalog remains in memory for the lifetime of the server process.

### Testing
- Added Gujarati parser/normalization regression tests.
- Verified clean CMake build and CTest suite locally with Tesseract 5.5.0 and Leptonica 1.84.1.

### Not claimed
- No 95% accuracy claim is made without a representative labeled Gujarati benchmark.
- A dedicated handwritten Gujarati OCR model is still a future model-quality step.
