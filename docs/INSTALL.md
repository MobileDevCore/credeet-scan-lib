# Installation

## Debian/Ubuntu
Install C compiler, CMake, pkg-config, Tesseract development files, Leptonica development files and language packs for English/Hindi/Gujarati. Then run:
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
Python API:
```bash
python -m pip install -r server/requirements.txt
python run_server.py
```
