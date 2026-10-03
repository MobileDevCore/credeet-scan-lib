from fastapi import FastAPI, UploadFile, File, Form, HTTPException
from pydantic import BaseModel
from pathlib import Path
from typing import Optional
import os, time, uuid, logging, threading
from python.scanmatch_ctypes import ScanMatch
from server.ocr_service import get_ocr_engine

app = FastAPI(
    title="ScanMatch REST API",
    version="0.11.0",
    description="FastAPI service for Gujarati/Hindi/English shopping-list OCR and product matching powered by libscanmatch"
)

ROOT = Path(__file__).resolve().parents[1]
LIB_DIR = ROOT / "build"
CATALOG = ROOT / "data" / "products.csv"
MAX_BYTES = 10 * 1024 * 1024  # 10 MB maximum image upload

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
log = logging.getLogger("scanmatch")

_engine = None
_init_lock = threading.Lock()
_ocr_engine = get_ocr_engine()

def get_engine():
    global _engine
    if _engine is None:
        with _init_lock:
            if _engine is None:
                candidates = ["libscanmatch.so", "libscanmatch.dll", "scanmatch.dll"]
                lib_path = None
                for c in candidates:
                    p = LIB_DIR / c
                    if p.exists():
                        lib_path = p
                        break
                if not lib_path:
                    lib_path = LIB_DIR / ("scanmatch.dll" if os.name == "nt" else "libscanmatch.so")

                engine = ScanMatch(lib_path)
                engine.load_catalog(CATALOG)
                _engine = engine
                log.info(f"Loaded ScanMatch engine from {lib_path} with catalog {CATALOG}")
    return _engine

class MatchRequest(BaseModel):
    ocr_text: str

@app.get("/health")
def health():
    """Health check endpoint."""
    engine = get_engine()
    return {
        "success": True,
        "ok": True,
        "version": "0.11.0",
        "catalog": "loaded" if engine else "error",
        "ocr_engine": _ocr_engine.name,
        "cloud_vision_configured": False
    }

@app.post("/api/v1/match")
def match_text(req: MatchRequest):
    """Direct text matching endpoint: ingests pre-extracted OCR text and matches against catalog."""
    rid = "REQ-" + uuid.uuid4().hex[:12]
    t0 = time.perf_counter()
    engine = get_engine()

    # Lock-free concurrent matching: C engine context is read-only after catalog load
    result = engine.process_text(req.ocr_text)

    result["request_id"] = rid
    result["server_time_ms"] = round((time.perf_counter() - t0) * 1000, 2)
    return result

@app.post("/api/v1/scan")
@app.post("/scan")
async def scan(
    file: Optional[UploadFile] = File(None),
    text: Optional[str] = Form(None),
    mock_ocr_text: Optional[str] = Form(None)
):
    """Primary endpoint for shopping-list images.
    
    1. Extracts text from image using active OCR interface (MockOCR locally).
    2. Passes extracted text to libscanmatch C library via Python FFI.
    3. Normalizes Gujarati/Hindi/English, parses quantities/units, and matches products.
    """
    rid = "REQ-" + uuid.uuid4().hex[:12]
    t0 = time.perf_counter()

    # Direct text bypass if text was provided in form
    if text:
        engine = get_engine()
        result = engine.process_text(text)
        result["request_id"] = rid
        result["ocr_engine"] = "direct_text"
        result["server_time_ms"] = round((time.perf_counter() - t0) * 1000, 2)
        return result

    if not file:
        raise HTTPException(status_code=400, detail={
            "success": False,
            "error": {"code": "NO_IMAGE", "message": "No image file was uploaded."}
        })

    # Validate image mime type
    content_type = file.content_type or ""
    if not (content_type.startswith("image/") or content_type == "application/octet-stream"):
        raise HTTPException(status_code=400, detail={
            "success": False,
            "error": {"code": "UNSUPPORTED_FORMAT", "message": "Uploaded file is not a supported image."}
        })

    data = await file.read()
    if not data:
        raise HTTPException(status_code=400, detail={
            "success": False,
            "error": {"code": "EMPTY_IMAGE", "message": "Uploaded image file is empty."}
        })

    if len(data) > MAX_BYTES:
        raise HTTPException(status_code=413, detail={
            "success": False,
            "error": {"code": "IMAGE_TOO_LARGE", "message": "Image exceeds maximum allowed size (10 MB)."}
        })

    # Step 1: OCR Extraction Layer via OCR Interface
    t_ocr_start = time.perf_counter()
    try:
        ocr_result = _ocr_engine.extract_text(
            data,
            filename=file.filename,
            mock_override=mock_ocr_text
        )
    except Exception as e:
        log.exception(f"OCR error for request {rid}: {e}")
        raise HTTPException(status_code=502, detail={
            "success": False,
            "error": {"code": "OCR_FAILED", "message": f"OCR processing failed: {str(e)}"}
        })
    ocr_time_ms = round((time.perf_counter() - t_ocr_start) * 1000, 2)

    if ocr_result.error:
        raise HTTPException(status_code=502, detail={
            "success": False,
            "error": {"code": "OCR_FAILED", "message": f"OCR failure: {ocr_result.error}"}
        })

    if not ocr_result.text or not ocr_result.text.strip():
        return {
            "success": False,
            "request_id": rid,
            "error": {
                "code": "NO_TEXT_DETECTED",
                "message": "No readable text was detected in the image."
            },
            "ocr_engine": _ocr_engine.name,
            "ocr_time_ms": ocr_time_ms,
            "server_time_ms": round((time.perf_counter() - t0) * 1000, 2),
            "items": []
        }

    # Step 2: C Product Processing & Matching Layer (libscanmatch) - lock-free
    engine = get_engine()
    result = engine.process_text(ocr_result.text)

    result["request_id"] = rid
    result["ocr_engine"] = _ocr_engine.name
    result["ocr_confidence"] = ocr_result.confidence
    result["ocr_time_ms"] = ocr_time_ms
    result["server_time_ms"] = round((time.perf_counter() - t0) * 1000, 2)

    return result
