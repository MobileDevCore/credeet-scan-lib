from fastapi import FastAPI, UploadFile, File, Form, HTTPException, Body
from pydantic import BaseModel
from pathlib import Path
from typing import Optional
import os, time, uuid, logging, threading
from python.scanmatch_ctypes import ScanMatch

app = FastAPI(title="ScanMatch C REST API", version="0.11.0")

ROOT = Path(__file__).resolve().parents[1]
LIB_DIR = ROOT / "build"
CATALOG = ROOT / "data" / "products.csv"

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
log = logging.getLogger("scanmatch")

_engine = None
_lock = threading.Lock()

def get_engine():
    global _engine
    if _engine is None:
        # Check libscanmatch.so, libscanmatch.dll, scanmatch.dll
        candidates = ["libscanmatch.so", "libscanmatch.dll", "scanmatch.dll"]
        lib_path = None
        for c in candidates:
            p = LIB_DIR / c
            if p.exists():
                lib_path = p
                break
        if not lib_path:
            lib_path = LIB_DIR / ("scanmatch.dll" if os.name == "nt" else "libscanmatch.so")

        _engine = ScanMatch(lib_path)
        _engine.load_catalog(CATALOG)
    return _engine

class MatchRequest(BaseModel):
    ocr_text: str

@app.get("/health")
def health():
    return {
        "success": True,
        "ok": True,
        "version": "0.11.0",
        "catalog": "loaded" if _engine else "lazy"
    }

@app.post("/api/v1/match")
def match_text(req: MatchRequest):
    """Processes OCR text through libscanmatch.so and returns structured product matches."""
    rid = "REQ-" + uuid.uuid4().hex[:12]
    t0 = time.perf_counter()
    engine = get_engine()

    with _lock:
        result = engine.process_text(req.ocr_text)

    result["request_id"] = rid
    result["server_time_ms"] = round((time.perf_counter() - t0) * 1000, 2)
    return result

@app.post("/api/v1/scan")
@app.post("/scan")
async def scan(
    file: Optional[UploadFile] = File(None),
    text: Optional[str] = Form(None)
):
    """Entry point for image OCR text matching.
    
    If text is provided directly, it is immediately processed.
    If an image file is provided, it notes that Google Cloud Vision integration
    is pending explicit human approval and credential configuration.
    """
    rid = "REQ-" + uuid.uuid4().hex[:12]
    t0 = time.perf_counter()

    if text:
        engine = get_engine()
        with _lock:
            result = engine.process_text(text)
        result["request_id"] = rid
        result["server_time_ms"] = round((time.perf_counter() - t0) * 1000, 2)
        return result

    if not file:
        raise HTTPException(status_code=400, detail={
            "success": False,
            "error": {"code": "NO_INPUT", "message": "Neither file nor text was provided."}
        })

    # When image is uploaded, note Cloud Vision approval status
    return {
        "success": False,
        "request_id": rid,
        "error": {
            "code": "CLOUD_VISION_PENDING_APPROVAL",
            "message": "Google Cloud Vision OCR requires user approval and credential configuration. "
                       "Use /api/v1/match with ocr_text directly to match products."
        }
    }
