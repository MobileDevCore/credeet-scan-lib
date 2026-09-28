#!/usr/bin/env python3
"""Generate a labeled test-set manifest. Photos must be supplied/collected separately."""
import json, pathlib
out=pathlib.Path('data/test_dataset.jsonl')
rows=[]
examples=[('English printed','Rice 5 kg','SKU001',5,'kg'),('English handwritten','Soap x 4','SKU004',4,'piece'),('English mixed','Shampoo 2 bottles','SKU005',2,'bottle'),('English typo','Rce 2 kg','SKU001',2,'kg'),('Gujarati','ચોખા ૫ કિલો','SKU001',5,'kg'),('Gujarati','સાબુ ૪','SKU004',4,'piece'),('Hindi','चावल 5 किलो','SKU001',5,'kg'),('Hindi','साबुन 4','SKU004',4,'piece'),('Mixed','clinic plus shampoo 2 bottle','SKU006',2,'bottle'),('Brand','parle g 2 packet','SKU007',2,'packet')]
for i,(kind,text,sku,q,u) in enumerate(examples,1): rows.append({'id':f'TEST-{i:03d}','category':kind,'text':text,'truth_product_id':sku,'truth_quantity':q,'truth_unit':u,'image_path':f'images/TEST-{i:03d}.jpg'})
out.write_text('\n'.join(json.dumps(x,ensure_ascii=False) for x in rows)+'\n',encoding='utf-8')
print(out)
