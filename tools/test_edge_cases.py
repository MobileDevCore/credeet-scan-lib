import sys, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
if hasattr(sys.stdout, 'reconfigure'):
    try: sys.stdout.reconfigure(encoding='utf-8')
    except Exception: pass
from python.scanmatch_ctypes import ScanMatch

sm = ScanMatch('build/libscanmatch.dll')
sm.load_catalog('data/products.csv')

edge_cases = [
    ('exact match', 'Sugar 1 kg'),
    ('alias match', 'ચોખા ૫ કિલો'),
    ('transliteration', 'Soyu been 750 g'),
    ('OCR error', 'suqar 1 kg'),
    ('Gujarati digits', 'દૂધ ૧ લિટર'),
    ('mixed language', '5 kg ચોખા'),
    ('missing quantity', 'ચોખા'),
    ('missing unit', 'ચોખા ૨'),
    ('unknown item', 'XYZRandomWidget 5 pcs'),
    ('ambiguous item', 'Soap 1 piece'),
    ('empty text', '   \n  \t  ')
]

for name, tc in edge_cases:
    res = sm.process_text(tc)
    items = res.get('items', [])
    err = res.get('error')
    if err:
        print(f'{name}: error_code={err.get("code")}')
    else:
        it = items[0]
        pid = it.get('product_id')
        st = it.get('status')
        q = it.get('quantity')
        u = it.get('unit')
        cands = len(it.get('candidates', []))
        print(f'{name}: id={pid} | status={st} | qty={q} | unit={u} | candidates={cands}')
