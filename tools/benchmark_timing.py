import time
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.stdout.reconfigure(encoding='utf-8')

from python.scanmatch_ctypes import ScanMatch
from fastapi.testclient import TestClient
from server.app import app

input_text = """10 રૂ. ચોકો ફ્લેક્સ
૨ કિગ્રા મેંદો
૨૫૦ ગ્રામ પામ ઓઇલ
૫ નંગ સંતુર સાબુ
૧૦ રૂ. ની ધાણાદાળ
100 gram beans soy"""

# 1. Startup & Catalog Loading Time
t0 = time.perf_counter()
engine = ScanMatch()
t_init = (time.perf_counter() - t0) * 1000

t0 = time.perf_counter()
engine.load_catalog(ROOT / "data" / "products.csv")
t_catalog = (time.perf_counter() - t0) * 1000

# Warm up
engine.process_text(input_text)

# 2. C Core Engine matching time (5 iterations)
runs = 5
t0 = time.perf_counter()
for _ in range(runs):
    res = engine.process_text(input_text)
t_c_total = ((time.perf_counter() - t0) / runs) * 1000

# Per line timing (5 iterations each)
lines = [l.strip() for l in input_text.splitlines() if l.strip()]
line_timings = []
for l in lines:
    t0 = time.perf_counter()
    for _ in range(5):
        engine.process_text(l)
    t_avg = ((time.perf_counter() - t0) / 5) * 1000
    line_timings.append((l, t_avg))

# 3. HTTP FastAPI REST API Endpoint time (/api/v1/match)
client = TestClient(app)
client.post("/api/v1/match", json={"ocr_text": input_text})

api_runs = 5
t0 = time.perf_counter()
for _ in range(api_runs):
    r = client.post("/api/v1/match", json={"ocr_text": input_text})
t_http_match = ((time.perf_counter() - t0) / api_runs) * 1000

# 4. HTTP /api/v1/scan (Mock OCR + C Engine + JSON HTTP Response)
client.post("/api/v1/scan", files={"file": ("test.jpg", input_text.encode("utf-8"), "image/jpeg")})
t0 = time.perf_counter()
for _ in range(api_runs):
    r = client.post("/api/v1/scan", files={"file": ("test.jpg", input_text.encode("utf-8"), "image/jpeg")})
t_http_scan = ((time.perf_counter() - t0) / api_runs) * 1000

print(f"=== BENCHMARK TIMING BREAKDOWN ===")
print(f"1. Catalog CSV Load Time (191 products): {t_catalog:.2f} ms")
print(f"2. C Core Engine Total Processing (All 6 Items): {t_c_total:.2f} ms ({t_c_total*1000:.0f} µs)")
print(f"3. C Core Engine Average Time Per Item: {t_c_total/len(lines):.2f} ms ({(t_c_total/len(lines))*1000:.0f} µs)")
print("\nPer-Item Detailed Breakdown:")
for item_text, ms in line_timings:
    print(f"   • \"{item_text}\": {ms:.2f} ms ({ms*1000:.0f} µs)")

print(f"\n4. Full FastAPI REST API Request (/api/v1/match): {t_http_match:.2f} ms")
print(f"5. Full Multipart Scan Request (/api/v1/scan): {t_http_scan:.2f} ms")
