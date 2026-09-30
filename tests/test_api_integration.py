import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from fastapi.testclient import TestClient
from server.app import app

client = TestClient(app)

def test_health():
    res = client.get("/health")
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    assert data["ok"] is True
    assert data["version"] == "0.11.0"
    assert data["catalog"] == "loaded"

def test_match_english():
    res = client.post("/api/v1/match", json={"ocr_text": "Rice 2 kg"})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    item = data["items"][0]
    assert item["product_id"] == "RICE001"
    assert item["quantity"] == 2.0
    assert item["unit"] == "kg"
    assert item["status"] == "confirmed"

def test_match_gujarati():
    res = client.post("/api/v1/match", json={"ocr_text": "ચોખા ૨ કિલો"})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    item = data["items"][0]
    assert item["product_id"] == "RICE001"
    assert item["quantity"] == 2.0
    assert item["unit"] == "kg"
    assert item["status"] == "confirmed"

def test_match_mixed():
    res = client.post("/api/v1/match", json={"ocr_text": "5 kg ચોખા"})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    item = data["items"][0]
    assert item["product_id"] == "RICE001"
    assert item["quantity"] == 5.0
    assert item["unit"] == "kg"
    assert item["status"] == "confirmed"

def test_match_hindi():
    res = client.post("/api/v1/match", json={"ocr_text": "चावल 2 किलो"})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    item = data["items"][0]
    assert item["product_id"] == "RICE001"
    assert item["quantity"] == 2.0
    assert item["unit"] == "kg"
    assert item["status"] == "confirmed"

def test_match_missing_quantity():
    # Must NOT invent quantity!
    res = client.post("/api/v1/match", json={"ocr_text": "ચોખા"})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    item = data["items"][0]
    assert item["product_id"] == "RICE001"
    assert item["quantity"] is None
    assert item["has_quantity"] is False
    assert item["unit"] is None
    assert item["has_unit"] is False

def test_match_missing_unit():
    # Must NOT invent unit!
    res = client.post("/api/v1/match", json={"ocr_text": "ચોખા ૨"})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    item = data["items"][0]
    assert item["product_id"] == "RICE001"
    assert item["quantity"] == 2.0
    assert item["has_quantity"] is True
    assert item["unit"] is None
    assert item["has_unit"] is False

def test_match_unknown_product():
    res = client.post("/api/v1/match", json={"ocr_text": "NonExistentThing 5 kg"})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    item = data["items"][0]
    assert item["product_id"] is None
    assert item["status"] == "unidentified"
    assert item["needs_confirmation"] is True

def test_match_no_ocr_text():
    res = client.post("/api/v1/match", json={"ocr_text": "   \n  \t  "})
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is False
    assert data["error"]["code"] == "NO_TEXT_DETECTED"

def test_scan_image_mock():
    # Test uploading image bytes to /api/v1/scan
    dummy_image = b"\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x01\x00\x60\x00\x60\x00\x00\xFF\xDB\x00C\x00"
    res = client.post(
        "/api/v1/scan",
        files={"file": ("rice_list.jpg", dummy_image, "image/jpeg")}
    )
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    assert data["ocr_backend"] == "mock"
    assert len(data["items"]) > 0
    assert data["items"][0]["product_id"] == "RICE001"

def test_scan_image_with_custom_mock_text():
    # Pass explicit mock text override
    dummy_image = b"\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x01\x00\x60\x00\x60\x00\x00\xFF\xDB\x00C\x00"
    res = client.post(
        "/api/v1/scan",
        files={"file": ("scan.jpg", dummy_image, "image/jpeg")},
        data={"mock_ocr_text": "દૂધ ૨ લિટર\nસાબુ ૪ નંગ"}
    )
    assert res.status_code == 200
    data = res.json()
    assert data["success"] is True
    assert len(data["items"]) == 2
    assert data["items"][0]["product_id"] == "MILK001"
    assert data["items"][0]["quantity"] == 2.0
    assert data["items"][0]["unit"] == "liter"
    assert data["items"][1]["product_id"] == "SOAP001"
    assert data["items"][1]["quantity"] == 4.0
    assert data["items"][1]["unit"] == "piece"

def test_scan_empty_image():
    res = client.post(
        "/api/v1/scan",
        files={"file": ("empty.jpg", b"", "image/jpeg")}
    )
    assert res.status_code == 400
    assert res.json()["detail"]["error"]["code"] == "EMPTY_IMAGE"

if __name__ == "__main__":
    test_health()
    test_match_english()
    test_match_gujarati()
    test_match_mixed()
    test_match_hindi()
    test_match_missing_quantity()
    test_match_missing_unit()
    test_match_unknown_product()
    test_match_no_ocr_text()
    test_scan_image_mock()
    test_scan_image_with_custom_mock_text()
    test_scan_empty_image()
    print("ALL PYTHON FASTAPI INTEGRATION TESTS PASSED SUCCESSFULLY!")
