import os, sys, time, json
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

if hasattr(os, 'add_dll_directory'):
    for p in ['C:/tools/msys64/ucrt64/bin', 'C:/Users/DEEP/Desktop/Scanmatch/build']:
        if os.path.exists(p):
            os.add_dll_directory(p)

from python.scanmatch_ctypes import ScanMatch

print("=== STEP 12: REAL PERFORMANCE MEASUREMENTS ===")

lib_path = 'build/libscanmatch.dll' if os.path.exists('build/libscanmatch.dll') else 'build/scanmatch.dll'
sm = ScanMatch(lib_path)
sm.load_catalog('data/products.csv')

benchmark_images = [
    ("images/eng_sugar_2kg.png", "Single-line English ('Sugar 2 kg')", "eng"),
    ("images/mix_rice_5kg.png", "Single-line Mixed ('ચોખા 5 kg')", "guj+eng"),
    ("images/mix_soap_4pcs.png", "Single-line Mixed ('Soap 4 pcs')", "eng"),
    ("images/guj_soap_ascii_4pcs.png", "Single-line Gujarati ('સાબુ 4 નંગ')", "guj"),
    ("images/multiline_shopping_list.png", "5-Item Multi-line Gujarati list", "guj"),
]

results = []

for img_path, label, lang in benchmark_images:
    t_start = time.perf_counter()
    res = sm.scan(img_path, languages=lang)
    t_end = time.perf_counter()
    wall_ms = (t_end - t_start) * 1000

    ocr_ms = res.get("ocr_time_ms", 0)
    match_ms = res.get("matching_time_ms", 0)
    total_engine_ms = res.get("total_time_ms", 0)
    parse_and_overhead_ms = max(0.0, total_engine_ms - (ocr_ms + match_ms))

    metrics = {
        "label": label,
        "image": img_path,
        "ocr_time_ms": ocr_ms,
        "matching_time_ms": match_ms,
        "parse_and_ffi_overhead_ms": round(parse_and_overhead_ms, 2),
        "total_engine_ms": total_engine_ms,
        "wall_clock_ms": round(wall_ms, 2),
        "items_detected": len(res.get("items", [])),
    }
    results.append(metrics)
    print(f"[{label}]")
    print(f"   OCR time: {ocr_ms} ms | Matching time: {match_ms} ms | Total C Engine: {total_engine_ms} ms | Wall-clock: {wall_ms:.2f} ms")

sm.close()

with open("tools/performance_results.json", "w", encoding="utf-8") as f:
    json.dump(results, f, indent=2)

print("\nPerformance results saved to tools/performance_results.json")
