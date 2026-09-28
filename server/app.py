from fastapi import FastAPI, UploadFile, File, HTTPException
from pathlib import Path
import tempfile, os, time, uuid, logging, threading
from python.scanmatch_ctypes import ScanMatch
app=FastAPI(title="ScanMatch C",version="0.10.1")
ROOT=Path(__file__).resolve().parents[1]
if os.name=="nt": name="scanmatch.dll"
elif os.name=="darwin": name="libscanmatch.dylib"
else: name="libscanmatch.so"
LIB=ROOT/"build"/name
CATALOG=ROOT/"data"/"products.csv"
MAX_BYTES=10*1024*1024
MAX_DIMENSION=10000
logging.basicConfig(level=logging.INFO,format='%(asctime)s %(levelname)s %(message)s')
log=logging.getLogger("scanmatch")
_engine=None
_lock=threading.Lock()

def err(code,msg,status=400): return HTTPException(status_code=status,detail={"success":False,"error":{"code":code,"message":msg}})
def get_engine():
    global _engine
    if _engine is None:
        _engine=ScanMatch(LIB);_engine.load_catalog(CATALOG)
    return _engine
@app.get("/health")
def health(): return {"success":True,"ok":True,"version":"0.10.1","catalog":"loaded" if _engine else "lazy"}
@app.post("/api/v1/scan")
@app.post("/scan")
async def scan(file: UploadFile=File(...),languages:str="guj+eng+hin"):
    rid="REQ-"+uuid.uuid4().hex[:12];t0=time.perf_counter()
    if not file: raise err("NO_IMAGE","No image was uploaded.")
    if not file.content_type or not file.content_type.startswith("image/"): raise err("UNSUPPORTED_FORMAT","Please upload a supported image file.")
    data=await file.read()
    if not data: raise err("EMPTY_IMAGE","The uploaded image is empty.")
    if len(data)>MAX_BYTES: raise err("IMAGE_TOO_LARGE","Image exceeds the 10 MB limit.",413)
    suffix=Path(file.filename or "image.jpg").suffix.lower()
    if suffix not in {".jpg",".jpeg",".png",".webp",".tif",".tiff",".bmp"}: raise err("UNSUPPORTED_FORMAT","Unsupported image extension.")
    fd,tmp=tempfile.mkstemp(prefix="scanmatch_",suffix=suffix);os.close(fd)
    try:
        Path(tmp).write_bytes(data)
        engine=get_engine()
        with _lock: result=engine.scan(tmp,languages)
        result["request_id"]=rid;result["file_size_bytes"]=len(data);result["server_time_ms"]=round((time.perf_counter()-t0)*1000,2)
        if not result.get("items"):
            result.update(success=False,error={"code":"NO_TEXT_DETECTED","message":"No readable text was detected in the image."})
        log.info("request_id=%s image_size=%d status=%s total_ms=%s",rid,len(data),result.get("success"),result.get("total_time_ms"))
        return result
    except RuntimeError as e:
        msg=str(e);code="OCR_FAILED" if "OCR" in msg else "PROCESSING_ERROR";log.exception("request_id=%s error=%s",rid,msg);raise err(code,msg,500)
    except Exception:
        log.exception("request_id=%s unexpected error",rid);raise err("SERVER_ERROR","Unexpected ScanMatch processing error.",500)
    finally:
        try: os.unlink(tmp)
        except OSError: pass
