# Installation

## Debian/Ubuntu
Install C compiler (GCC/Clang), CMake, and standard build tools:
```bash
sudo apt update && sudo apt install -y build-essential cmake
```
Build C library and run test suite:
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
