import os, sys, tempfile, glob, time, json
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

# Add MSYS2 UCRT64 toolchain DLL directory for Python 3.8+ Windows ctypes
if hasattr(os, 'add_dll_directory'):
    for p in ['C:/tools/msys64/ucrt64/bin', 'C:/Users/DEEP/Desktop/Scanmatch/build']:
        if os.path.exists(p):
            os.add_dll_directory(p)

from fastapi.testclient import TestClient
from server.app import app

client = TestClient(app)

print("=== STEP 11: FASTAPI SERVER ENDPOINT TESTING ===")

# Test 1: GET /health
print("\n[1] Testing GET /health...")
resp = client.get("/health")
print(f"Status: {resp.status_code}")
print(f"Response: {resp.json()}")
assert resp.status_code == 200
assert resp.json().get("ok") is True

# Count temp files before
temp_dir = tempfile.gettempdir()
temp_before = set(glob.glob(os.path.join(temp_dir, "scanmatch_*")))

# Test 2: POST /api/v1/scan with English image
print("\n[2] Testing POST /api/v1/scan with 'images/eng_sugar_2kg.png'...")
img_path = Path("images/eng_sugar_2kg.png")
with open(img_path, "rb") as f:
    resp = client.post("/api/v1/scan", files={"file": ("eng_sugar_2kg.png", f, "image/png")}, params={"languages": "eng"})
print(f"Status: {resp.status_code}")
res_json = resp.json()
print(f"Response JSON: {json.dumps(res_json, indent=2, ensure_ascii=False)}")
assert resp.status_code == 200
assert res_json.get("success") is True
assert len(res_json.get("items", [])) > 0

# Test 3: POST /api/v1/scan with Gujarati multi-line image
print("\n[3] Testing POST /api/v1/scan with 'images/multiline_shopping_list.png'...")
img_path = Path("images/multiline_shopping_list.png")
with open(img_path, "rb") as f:
    resp = client.post("/api/v1/scan", files={"file": ("multiline_shopping_list.png", f, "image/png")}, params={"languages": "guj"})
print(f"Status: {resp.status_code}")
res_json = resp.json()
print(f"Response JSON: {json.dumps(res_json, indent=2, ensure_ascii=False)}")
assert resp.status_code == 200

# Test 4: Error handling - upload invalid file format
print("\n[4] Testing Error Handling: Non-image upload...")
resp = client.post("/api/v1/scan", files={"file": ("test.txt", b"not an image", "text/plain")})
print(f"Status: {resp.status_code}")
print(f"Response: {resp.json()}")
assert resp.status_code == 400

# Test 5: Verify temporary files cleaned up
temp_after = set(glob.glob(os.path.join(temp_dir, "scanmatch_*")))
leaked = temp_after - temp_before
print(f"\n[5] Temporary file cleanup verification: leaked files = {len(leaked)}")
assert len(leaked) == 0, f"Leaked temp files: {leaked}"
print("Temporary file cleanup: VERIFIED CLEAN (0 leaked files).")
