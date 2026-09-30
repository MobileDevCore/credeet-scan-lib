"""Google Cloud Vision OCR service and Local Mock adapter for ScanMatch.

Architecture:
FastAPI (/api/v1/scan)
  ↓
OCRService.extract_text(image_bytes)
  ├─ If GOOGLE_APPLICATION_CREDENTIALS / GOOGLE_API_KEY present:
  │    Calls Google Cloud Vision API with 5.0s timeout & transient retry
  │    Extracts full text and confidence
  └─ If no credentials (local / test mode):
       Uses safe local mock OCR producing representative Gujarati/multilingual text
  ↓
Returns (raw_ocr_text, confidence) to FastAPI
  ↓
FastAPI sends raw_ocr_text into libscanmatch.so via Python FFI
"""

import base64
import json
import logging
import os
import urllib.error
import urllib.request
from typing import Optional, Tuple

log = logging.getLogger("scanmatch.ocr")

DEFAULT_MOCK_TEXT = (
    "ચોખા ૨ કિલો\n"
    "દૂધ ૧ લિટર\n"
    "સાબુ ૪ નંગ\n"
    "Rice 5 kg\n"
    "ચા ૫૦૦ g"
)

class OCRService:
    def __init__(self, mode: Optional[str] = None):
        """Initializes the OCR service.
        
        mode: 'auto', 'live', or 'mock'.
        Defaults to 'auto' (checks for credentials; falls back to 'mock').
        """
        self.mode = mode or os.getenv("SCANMATCH_OCR_MODE", "auto")
        self.api_key = os.getenv("GOOGLE_API_KEY")
        self.credentials_path = os.getenv("GOOGLE_APPLICATION_CREDENTIALS")
        self.backend = "mock"
        self._init_backend()

    def _init_backend(self):
        if self.mode == "mock":
            self.backend = "mock"
            log.info("OCR Service running in explicit MOCK mode.")
            return

        if self.api_key or (self.credentials_path and os.path.exists(self.credentials_path)):
            self.backend = "google_cloud_vision"
            log.info("OCR Service configured for live Google Cloud Vision.")
        else:
            self.backend = "mock"
            log.info("No Google Cloud credentials found. Running in safe local MOCK mode.")

    def extract_text(
        self,
        image_bytes: bytes,
        filename: Optional[str] = None,
        mock_override_text: Optional[str] = None
    ) -> Tuple[str, float]:
        """Extracts text and mean confidence from image bytes.
        
        Returns:
            (raw_ocr_text, ocr_confidence)
        """
        if not image_bytes:
            return "", 0.0

        if mock_override_text is not None:
            return mock_override_text, 0.95

        if self.backend == "google_cloud_vision":
            return self._call_cloud_vision(image_bytes)
        else:
            return self._call_mock(image_bytes, filename)

    def _call_mock(self, image_bytes: bytes, filename: Optional[str] = None) -> Tuple[str, float]:
        """Local mock OCR for development and zero-cloud testing."""
        # If the file passed is actually a UTF-8 text file disguised as an image for testing,
        # extract its text directly.
        try:
            decoded = image_bytes.decode('utf-8')
            if any(ord(c) > 127 for c in decoded) or any(w in decoded.lower() for w in ["kg", "rice", "soap"]):
                return decoded.strip(), 0.95
        except UnicodeDecodeError:
            pass

        # If filename gives a hint:
        if filename:
            fn = filename.lower()
            if "rice" in fn:
                return "Rice 2 kg", 0.98
            if "gujarati" in fn or "chokha" in fn:
                return "ચોખા ૫ કિલો\nદૂધ ૧ લિટર", 0.96
            if "hindi" in fn:
                return "चावल 2 किलो\nदूध 1 लीटर", 0.95
            if "empty" in fn:
                return "", 0.0

        return DEFAULT_MOCK_TEXT, 0.95

    def _call_cloud_vision(self, image_bytes: bytes) -> Tuple[str, float]:
        """Executes synchronous Google Cloud Vision OCR request with timeout and transient retry."""
        b64_content = base64.b64encode(image_bytes).decode("ascii")

        payload = {
            "requests": [
                {
                    "image": {"content": b64_content},
                    "features": [{"type": "DOCUMENT_TEXT_DETECTION", "maxResults": 1}],
                    "imageContext": {
                        "languageHints": ["gu", "hi", "en"]
                    }
                }
            ]
        }

        url = "https://vision.googleapis.com/v1/images:annotate"
        if self.api_key:
            url += f"?key={self.api_key}"

        headers = {"Content-Type": "application/json; charset=utf-8"}

        # If using service account token:
        token = self._get_service_account_token()
        if token:
            headers["Authorization"] = f"Bearer {token}"

        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")

        # Retry once on transient error
        max_attempts = 2
        for attempt in range(max_attempts):
            try:
                with urllib.request.urlopen(req, timeout=5.0) as resp:
                    if resp.status != 200:
                        raise RuntimeError(f"Cloud Vision API returned status {resp.status}")
                    body = json.loads(resp.read().decode("utf-8"))
                    return self._parse_vision_response(body)
            except urllib.error.HTTPError as e:
                if e.code in {500, 502, 503, 504, 429} and attempt < max_attempts - 1:
                    log.warning(f"Transient HTTP {e.code} from Cloud Vision. Retrying...")
                    continue
                err_text = e.read().decode("utf-8", errors="replace")
                raise RuntimeError(f"Cloud Vision API error ({e.code}): {err_text}")
            except Exception as e:
                if attempt < max_attempts - 1:
                    log.warning(f"Error calling Cloud Vision: {e}. Retrying...")
                    continue
                raise RuntimeError(f"Cloud Vision network error: {e}")

        return "", 0.0

    def _get_service_account_token(self) -> Optional[str]:
        """Extracts OAuth2 bearer token from Google Cloud environment if available."""
        # Dynamically imported so local mock mode and IDE linters do not require google-auth
        try:
            import importlib
            google_auth = importlib.import_module("google.auth")
            google_auth_transport = importlib.import_module("google.auth.transport.requests")
            creds, _ = google_auth.default(scopes=["https://www.googleapis.com/auth/cloud-platform"])
            auth_req = google_auth_transport.Request()
            creds.refresh(auth_req)
            return creds.token
        except Exception:
            return None

    def _parse_vision_response(self, body: dict) -> Tuple[str, float]:
        """Parses Google Cloud Vision response into raw text and average confidence."""
        responses = body.get("responses", [])
        if not responses:
            return "", 0.0

        first_resp = responses[0]
        if "error" in first_resp:
            err = first_resp["error"]
            raise RuntimeError(f"Cloud Vision annotation error ({err.get('code')}): {err.get('message')}")

        full_text = first_resp.get("fullTextAnnotation", {}).get("text", "")
        if not full_text:
            # Fallback to textAnnotations if fullTextAnnotation is missing
            text_annotations = first_resp.get("textAnnotations", [])
            if text_annotations:
                full_text = text_annotations[0].get("description", "")

        # Calculate mean symbol confidence if available
        confidences = []
        pages = first_resp.get("fullTextAnnotation", {}).get("pages", [])
        for page in pages:
            for block in page.get("blocks", []):
                if "confidence" in block:
                    confidences.append(float(block["confidence"]))
                for para in block.get("paragraphs", []):
                    if "confidence" in para:
                        confidences.append(float(para["confidence"]))

        mean_conf = (sum(confidences) / len(confidences)) if confidences else 0.90
        return full_text.strip(), mean_conf
