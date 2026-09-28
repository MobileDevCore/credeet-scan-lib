import ctypes, json, os

class ScanMatch:
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
                alt = os.path.join(lib_dir, "libscanmatch.dll") if "libscanmatch.dll" not in lib_str else os.path.join(lib_dir, "scanmatch.dll")
                if os.path.exists(alt):
                    lib_str = alt
        self.lib = ctypes.CDLL(lib_str)
        self.lib.sm_create_context.restype = ctypes.c_void_p
        self.lib.sm_destroy_context.argtypes = [ctypes.c_void_p]
        self.lib.sm_load_catalog_csv_ffi.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.lib.sm_load_catalog_csv_ffi.restype = ctypes.c_int
        self.lib.sm_scan_image_json.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_void_p)
        ]
        self.lib.sm_scan_image_json.restype = ctypes.c_int
        self.lib.sm_free_string.argtypes = [ctypes.c_void_p]
        self.lib.sm_last_error.argtypes = [ctypes.c_void_p]
        self.lib.sm_last_error.restype = ctypes.c_char_p
        self.ctx = self.lib.sm_create_context()
        if not self.ctx:
            raise RuntimeError("cannot create ScanMatch context")

    def close(self):
        if self.ctx:
            self.lib.sm_destroy_context(self.ctx)
            self.ctx = None

    def load_catalog(self, path):
        rc = self.lib.sm_load_catalog_csv_ffi(self.ctx, os.fspath(path).encode())
        if rc:
            raise RuntimeError(self.lib.sm_last_error(self.ctx).decode(errors="replace"))

    def scan(self, image_path, languages="eng+hin+guj"):
        p = ctypes.c_void_p()
        rc = self.lib.sm_scan_image_json(
            self.ctx, os.fspath(image_path).encode(),
            languages.encode(), ctypes.byref(p)
        )
        if rc:
            raise RuntimeError(self.lib.sm_last_error(self.ctx).decode(errors="replace"))
        try:
            raw = ctypes.string_at(p).decode("utf-8")
            return json.loads(raw)
        finally:
            self.lib.sm_free_string(p)
