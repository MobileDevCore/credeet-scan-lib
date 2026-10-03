import os, sys, json
if hasattr(sys.stdout, 'reconfigure'):
    try: sys.stdout.reconfigure(encoding='utf-8')
    except Exception: pass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

if hasattr(os, 'add_dll_directory'):
    for p in ['C:/tools/msys64/ucrt64/bin', str(ROOT / 'build')]:
        if os.path.exists(p):
            os.add_dll_directory(p)

from fastapi.testclient import TestClient
from server.app import app

client = TestClient(app)

print("=== FASTAPI SERVER ENDPOINT TESTING ===")

# Test 1: GET /health
print("\n[1] Testing GET /health...")
resp = client.get("/health")
print(f"Status: {resp.status_code}")
print(f"Response: {resp.json()}")
assert resp.status_code == 200
assert resp.json().get("ok") is True

# Test 2: POST /api/v1/match with text
print("\n[2] Testing POST /api/v1/match with Gujarati text...")
resp = client.post("/api/v1/match", json={"ocr_text": "ચોખા ૨ કિલો\nRice 5 kg"})
print(f"Status: {resp.status_code}")
res_json = resp.json()
print(f"Response JSON: {json.dumps(res_json, indent=2, ensure_ascii=False)}")
assert resp.status_code == 200
assert res_json.get("success") is True
assert len(res_json.get("items", [])) == 2

# Test 3: POST /api/v1/scan with mock image
print("\n[3] Testing POST /api/v1/scan with mock image...")
dummy_image = b"\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x01\x00\x60\x00\x60\x00\x00\xFF\xDB\x00C\x00"
resp = client.post("/api/v1/scan", files={"file": ("shopping_list.jpg", dummy_image, "image/jpeg")})
print(f"Status: {resp.status_code}")
res_json = resp.json()
print(f"Response JSON: {json.dumps(res_json, indent=2, ensure_ascii=False)}")
assert resp.status_code == 200
assert res_json.get("success") is True

# Test 4: Error handling - upload invalid file format
print("\n[4] Testing Error Handling: Non-image upload...")
resp = client.post("/api/v1/scan", files={"file": ("test.txt", b"not an image", "text/plain")})
print(f"Status: {resp.status_code}")
print(f"Response: {resp.json()}")
assert resp.status_code == 400

print("\nALL SERVER API TESTS PASSED SUCCESSFULLY!")
