#!/usr/bin/env python3
"""Evaluate ScanMatch JSONL records. Each line may contain prediction/truth, quantity/unit and candidates."""
import json,sys
if len(sys.argv)!=2:
 print("usage: python tools/evaluate.py predictions.jsonl");raise SystemExit(2)
n=sku=q=u=top3=0
for line in open(sys.argv[1],encoding="utf-8"):
 if not line.strip(): continue
 x=json.loads(line);n+=1
 pred=x.get("prediction");truth=x.get("truth");sku+=pred==truth
 if "quantity_prediction" in x: q+=x.get("quantity_prediction")==x.get("quantity_truth")
 if "unit_prediction" in x: u+=x.get("unit_prediction")==x.get("unit_truth")
 if truth and truth in x.get("candidates",[]): top3+=1
print(f"items={n}");print(f"sku_correct={sku}");print(f"sku_accuracy={(sku/n*100):.2f}%" if n else "sku_accuracy=N/A")
if any("quantity_prediction" in json.loads(z) for z in open(sys.argv[1],encoding="utf-8") if z.strip()): print(f"quantity_accuracy={(q/n*100):.2f}%")
if any("unit_prediction" in json.loads(z) for z in open(sys.argv[1],encoding="utf-8") if z.strip()): print(f"unit_accuracy={(u/n*100):.2f}%")
