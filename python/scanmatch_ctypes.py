import ctypes
import json
import os
from pathlib import Path

class ScanMatch:
    """Python FFI wrapper for libscanmatch.so / libscanmatch.dll.
    
    Safe memory management: Every C-allocated JSON buffer is guaranteed
    to be freed via sm_free_string in a finally block.
    """
    def __init__(self, library_path):
        lib_str = os.fspath(library_path)
        if os.name == "nt":
            lib_dir = os.path.dirname(os.path.abspath(lib_str))
            if hasattr(os, "add_dll_directory"):
                if os.path.isdir(lib_dir):
                    try: os.add_dll_directory(lib_dir)
                    except Exception: pass
                for extra in ["C:/tools/msys64/ucrt64/bin", "C:/msys64/ucrt64/bin", "C:/tools/msys64/mingw64/bin"]:
                    if os.path.isdir(extra):
                        try: os.add_dll_directory(extra)
                        except Exception: pass
            if not os.path.exists(lib_str):
                for candidate in ["libscanmatch.so", "libscanmatch.dll", "scanmatch.dll"]:
                    alt = os.path.join(lib_dir, candidate)
                    if os.path.exists(alt):
                        lib_str = alt
                        break

        self.lib = ctypes.CDLL(lib_str)

        # Declare exact C types
        self.lib.sm_create_context.argtypes = []
        self.lib.sm_create_context.restype = ctypes.c_void_p

        self.lib.sm_destroy_context.argtypes = [ctypes.c_void_p]
        self.lib.sm_destroy_context.restype = None

        self.lib.sm_load_catalog_csv_ffi.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.lib.sm_load_catalog_csv_ffi.restype = ctypes.c_int

        self.lib.sm_process_text_json.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_char_p)
        ]
        self.lib.sm_process_text_json.restype = ctypes.c_int

        self.lib.sm_last_error.argtypes = [ctypes.c_void_p]
        self.lib.sm_last_error.restype = ctypes.c_char_p

        self.lib.sm_free_string.argtypes = [ctypes.c_void_p]
        self.lib.sm_free_string.restype = None

        self.ctx = self.lib.sm_create_context()
        if not self.ctx:
            raise RuntimeError("Failed to create ScanMatch context")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        if self.ctx:
            self.lib.sm_destroy_context(self.ctx)
            self.ctx = None

    def load_catalog(self, path):
        """Loads product catalog CSV into ScanMatch context."""
        rc = self.lib.sm_load_catalog_csv_ffi(self.ctx, os.fspath(path).encode('utf-8'))
        if rc != 0:
            err = self.lib.sm_last_error(self.ctx)
            msg = err.decode('utf-8', errors='replace') if err else f"Code {rc}"
            raise RuntimeError(f"Catalog load error: {msg}")

    def process_text(self, ocr_text: str) -> dict:
        """Processes raw OCR text through ScanMatch C library.
        
        Performs Gujarati/Hindi/English normalization, extracts quantities/units,
        matches against product catalog, and returns structured result.
        """
        if not self.ctx:
            raise RuntimeError("ScanMatch context is closed")

        text_bytes = ocr_text.encode('utf-8') if ocr_text else b""
        out_ptr = ctypes.c_char_p()

        rc = self.lib.sm_process_text_json(self.ctx, text_bytes, ctypes.byref(out_ptr))
        if rc != 0:
            err = self.lib.sm_last_error(self.ctx)
            msg = err.decode('utf-8', errors='replace') if err else f"Code {rc}"
            raise RuntimeError(f"ScanMatch processing error: {msg}")

        try:
            if not out_ptr.value:
                return {"success": False, "error": {"code": "EMPTY_OUTPUT", "message": "No output returned from C library"}}
            raw_json = out_ptr.value.decode('utf-8')
            return json.loads(raw_json)
        finally:
            if out_ptr:
                self.lib.sm_free_string(ctypes.cast(out_ptr, ctypes.c_void_p))
