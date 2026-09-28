#!/usr/bin/env python3
"""Basic deployment benchmark: scan the same image N times and report timing."""
import argparse,json,subprocess,time,statistics
p=argparse.ArgumentParser();p.add_argument('image');p.add_argument('--catalog',default='data/products.csv');p.add_argument('-n',type=int,default=1);args=p.parse_args()
cmd=['build/scanmatch_cli',args.image,args.catalog]
ms=[];fail=0
for _ in range(args.n):
 t=time.perf_counter();r=subprocess.run(cmd,capture_output=True,text=True);dt=(time.perf_counter()-t)*1000
 if r.returncode: fail+=1
 else:
  try: ms.append(float(json.loads(r.stdout).get('total_time_ms',dt)))
  except Exception: ms.append(dt)
print(json.dumps({'images':args.n,'successful':len(ms),'failures':fail,'average_ms':statistics.mean(ms) if ms else None,'max_ms':max(ms) if ms else None},indent=2))
