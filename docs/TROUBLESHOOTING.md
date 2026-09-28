# Troubleshooting

- CMake says Tesseract missing: install the Tesseract development package and confirm `pkg-config --modversion tesseract` works.
- CMake says Leptonica missing: install the Leptonica development package and confirm `pkg-config --modversion lept` works.
- OCR fails: verify language packs (`eng`, `hin`, `guj`) exist and the image is readable.
- No text detected: retake the image with the full paper visible, good lighting and minimal blur.
- Low-confidence match: use the returned candidates for confirmation and improve catalog aliases/errors rather than forcing a match.
