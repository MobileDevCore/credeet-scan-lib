import os, sys, ctypes, json

if hasattr(os, 'add_dll_directory'):
    for p in ['C:/tools/msys64/ucrt64/bin', 'C:/Users/DEEP/Desktop/Scanmatch/build']:
        if os.path.exists(p):
            os.add_dll_directory(p)

lib_path = 'C:/Users/DEEP/Desktop/Scanmatch/build/libscanmatch.dll'
if not os.path.exists(lib_path):
    lib_path = 'C:/Users/DEEP/Desktop/Scanmatch/build/scanmatch.dll'

lib = ctypes.CDLL(lib_path)

class SM_ListItem(ctypes.Structure):
    _fields_ = [
        ('product_text', ctypes.c_char * 512),
        ('quantity', ctypes.c_double),
        ('unit', ctypes.c_char * 32),
        ('language', ctypes.c_char * 16),
        ('ocr_confidence', ctypes.c_double),
    ]

class SM_Product(ctypes.Structure):
    _fields_ = [
        ('sku', ctypes.c_char * 64),
        ('canonical_name', ctypes.c_char * 512),
        ('brand', ctypes.c_char * 64),
        ('category', ctypes.c_char * 64),
        ('aliases', (ctypes.c_char * 256) * 32),
        ('alias_count', ctypes.c_size_t),
        ('ocr_errors', (ctypes.c_char * 256) * 32),
        ('ocr_error_count', ctypes.c_size_t),
        ('units', (ctypes.c_char * 32) * 16),
        ('unit_count', ctypes.c_size_t),
        ('pack_sizes', (ctypes.c_char * 32) * 16),
        ('pack_size_count', ctypes.c_size_t),
    ]

class SM_ProductMatch(ctypes.Structure):
    _fields_ = [
        ('sku', ctypes.c_char * 64),
        ('canonical_name', ctypes.c_char * 512),
        ('score', ctypes.c_double),
        ('text_score', ctypes.c_double),
        ('token_score', ctypes.c_double),
        ('embedding_score', ctypes.c_double),
        ('exact_alias_score', ctypes.c_double),
        ('brand_score', ctypes.c_double),
        ('catalog_evidence', ctypes.c_double),
        ('confidence_band', ctypes.c_char * 16),
    ]

parse_line = lib.sm_parse_line
parse_line.argtypes = [ctypes.c_char_p, ctypes.POINTER(SM_ListItem)]
parse_line.restype = ctypes.c_int

load_catalog = lib.sm_load_catalog_csv
load_catalog.argtypes = [ctypes.c_char_p, ctypes.POINTER(SM_Product), ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
load_catalog.restype = ctypes.c_int

match_product = lib.sm_match_product_hybrid
match_product.argtypes = [
    ctypes.POINTER(SM_ListItem),
    ctypes.POINTER(SM_Product),
    ctypes.c_size_t,
    ctypes.POINTER(SM_ProductMatch),
    ctypes.c_size_t,
    ctypes.c_void_p,
    ctypes.c_void_p
]
match_product.restype = ctypes.c_int

print("=== STEP 8: QUANTITY AND UNIT PARSING ===")
test_step8 = [
    "2 kg rice",
    "Rice 5 kg",
    "rice 2kg",
    "Soap x 4",
    "Shampoo 2 bottles",
    "500 g badam",
    "ચોખા ૫ કિલો",
    "દૂધ ૨ લિટર",
    "સાબુ ૪ નંગ",
]

for tc in test_step8:
    item = SM_ListItem()
    rc = parse_line(tc.encode('utf-8'), ctypes.byref(item))
    res = {
        "input": tc,
        "product": item.product_text.decode('utf-8'),
        "quantity": item.quantity,
        "unit": item.unit.decode('utf-8'),
        "language": item.language.decode('utf-8'),
        "rc": rc
    }
    print(json.dumps(res, ensure_ascii=False))

print("\n=== STEP 7: PRODUCT MATCHING AND CATALOG VALIDATION ===")
products = (SM_Product * 4096)()
n_products = ctypes.c_size_t(0)
rc_cat = load_catalog("data/products.csv".encode('utf-8'), products, 4096, ctypes.byref(n_products))
print(f"Catalog loaded: rc={rc_cat}, count={n_products.value}")

match_cases = [
    ("ચોખા", "Exact Gujarati match"),
    ("ચોખ", "OCR typo 1 (ચોખ -> ચોખા)"),
    ("ચોકા", "OCR typo 2 (ચોકા -> ચોખા)"),
    ("સાબુ", "Generic product: Soap"),
    ("લક્સ સાબુ", "Branded product: Lux Soap"),
    ("શેમ્પૂ", "Generic shampoo"),
    ("ક્લિનિક પ્લસ", "Branded: Clinic Plus"),
    ("બાદામ", "Hindi/Gujarati Almonds"),
    ("unknown item 123", "Non-catalog item"),
]

for text, desc in match_cases:
    item = SM_ListItem()
    parse_line(text.encode('utf-8'), ctypes.byref(item))
    item.ocr_confidence = 0.95
    matches = (SM_ProductMatch * 5)()
    nm = match_product(ctypes.byref(item), products, n_products.value, matches, 5, None, None)
    best = matches[0] if nm > 0 else None
    top_candidates = [
        {"sku": matches[k].sku.decode('utf-8'), "name": matches[k].canonical_name.decode('utf-8'), "score": round(matches[k].score, 4), "band": matches[k].confidence_band.decode('utf-8')}
        for k in range(min(nm, 3))
    ]
    print(f"Query: '{text}' ({desc}) -> Best: {best.sku.decode('utf-8')} ({best.canonical_name.decode('utf-8')}), Score: {best.score:.4f}, Band: {best.confidence_band.decode('utf-8')}")
    print(f"   Candidates: {top_candidates}")
