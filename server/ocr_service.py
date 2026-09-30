"""Replaceable OCR interface and Mock implementation for ScanMatch.

TARGET ARCHITECTURE:
Image
  ↓
FastAPI (/api/v1/scan)
  ↓
BaseOCR interface (extract_text)
  ↓
MockOCR implementation (Local / offline deterministic OCR)
  ↓
OCR text & confidence
  ↓
Python FFI (ScanMatch.process_text)
  ↓
libscanmatch.so (normalization, parser, catalog matching)
  ↓
JSON response

FUTURE SEPARATION:
Later, MockOCR can be replaced with GoogleCloudVisionOCR implementing the same
BaseOCR interface without modifying libscanmatch.so, the Python FFI, or API response formats.
Google Cloud Vision is NOT configured or active in this phase.
"""

import logging
import os
from typing import NamedTuple, Optional

log = logging.getLogger("scanmatch.ocr")

class OCRResult(NamedTuple):
    """Standard OCR output structure returned by all OCR implementations."""
    text: str
    confidence: float
    success: bool
    error: Optional[str] = None

class BaseOCR:
    """Abstract interface for replaceable OCR implementations."""
    name: str = "base"

    def extract_text(
        self,
        image_bytes: bytes,
        filename: Optional[str] = None,
        mock_override: Optional[str] = None
    ) -> OCRResult:
        """Extracts text and confidence from raw image bytes.
        
        Args:
            image_bytes: Raw binary content of the uploaded image file.
            filename: Optional original filename (useful for fixture mapping in mock tests).
            mock_override: Optional explicit text override for testing.
            
        Returns:
            OCRResult containing extracted text, confidence, success flag, and optional error.
        """
        raise NotImplementedError

class MockOCR(BaseOCR):
    """Deterministic local mock OCR implementation for offline development and testing.
    
    Guarantees:
    - ZERO network calls
    - ZERO Google Cloud accounts or credentials required
    - ZERO billing or cloud resources used
    - Fully deterministic output mapped to project test cases
    """
    name: str = "mock"

    DEFAULT_TEXT = (
        "ચોખા ૫ કિલો\n"
        "દૂધ ૧ લિટર\n"
        "સાબુ ૪ નંગ\n"
        "Rice 2 kg\n"
        "ચા ૫૦૦ g"
    )

    def extract_text(
        self,
        image_bytes: bytes,
        filename: Optional[str] = None,
        mock_override: Optional[str] = None
    ) -> OCRResult:
        if not image_bytes:
            return OCRResult(text="", confidence=0.0, success=False, error="EMPTY_IMAGE_BYTES")

        # Explicit test override
        if mock_override is not None:
            text = mock_override.strip()
            return OCRResult(text=text, confidence=0.95, success=bool(text))

        # Check if the binary payload itself is a UTF-8 text fixture
        try:
            decoded = image_bytes.decode('utf-8')
            # If payload contains UTF-8 text (e.g. Gujarati script or shopping keywords)
            if any(ord(c) > 127 for c in decoded) or any(w in decoded.lower() for w in ["kg", "rice", "soap", "g", "liter"]):
                text = decoded.strip()
                return OCRResult(text=text, confidence=0.95, success=bool(text))
        except UnicodeDecodeError:
            pass

        # Deterministic mapping based on filename conventions for automated test suites
        if filename:
            fn = filename.lower()
            if "fail" in fn or "error" in fn:
                return OCRResult(text="", confidence=0.0, success=False, error="SIMULATED_OCR_FAILURE")
            if "empty" in fn or "blank" in fn:
                return OCRResult(text="", confidence=0.0, success=False, error=None)
            if "rice" in fn or "english" in fn:
                return OCRResult(text="Rice 2 kg", confidence=0.98, success=True)
            if "gujarati" in fn or "chokha" in fn:
                return OCRResult(text="ચોખા ૫ કિલો", confidence=0.98, success=True)
            if "mixed" in fn:
                return OCRResult(text="5 kg ચોખા", confidence=0.98, success=True)
            if "hindi" in fn or "chawal" in fn:
                return OCRResult(text="चावल 2 किलो", confidence=0.98, success=True)
            if "unknown" in fn:
                return OCRResult(text="XYZUnknownThing 10 pcs", confidence=0.90, success=True)
            if "missing_quantity" in fn:
                return OCRResult(text="ચોખા", confidence=0.95, success=True)
            if "missing_unit" in fn:
                return OCRResult(text="ચોખા ૨", confidence=0.95, success=True)
            if "multiline" in fn:
                return OCRResult(text="ચોખા ૫ કિલો\nદૂધ ૧ લિટર\nસાબુ ૪ નંગ\nRice 2 kg", confidence=0.95, success=True)

        return OCRResult(text=self.DEFAULT_TEXT, confidence=0.95, success=True)

class GoogleCloudVisionOCR(BaseOCR):
    """Future Google Cloud Vision OCR implementation.
    
    NOTE: Intentionally NOT IMPLEMENTED in Phase 6.
    Connecting this requires explicit human approval, Google account setup,
    billing activation, and credentials configuration.
    """
    name: str = "google_cloud_vision"

    def extract_text(
        self,
        image_bytes: bytes,
        filename: Optional[str] = None,
        mock_override: Optional[str] = None
    ) -> OCRResult:
        raise NotImplementedError(
            "Google Cloud Vision OCR is not yet configured. "
            "Local MockOCR is currently active. "
            "Human approval, billing, and credentials are required to activate Cloud Vision."
        )

def get_ocr_engine(engine_type: Optional[str] = None) -> BaseOCR:
    """Factory returning the active OCR implementation (defaults to MockOCR)."""
    selected = (engine_type or os.getenv("SCANMATCH_OCR_ENGINE", "mock")).lower()
    if selected in {"cloud_vision", "google_cloud_vision"}:
        return GoogleCloudVisionOCR()
    return MockOCR()
