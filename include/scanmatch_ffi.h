#ifndef SCANMATCH_FFI_H
#define SCANMATCH_FFI_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#ifdef SCANMATCH_BUILD
#define SM_API __declspec(dllexport)
#else
#define SM_API __declspec(dllimport)
#endif
#else
#define SM_API
#endif
typedef struct SM_Context SM_Context;
SM_API SM_Context *sm_create_context(void);
SM_API void sm_destroy_context(SM_Context *ctx);
SM_API int sm_load_catalog_csv_ffi(SM_Context *ctx,const char *path);
SM_API int sm_scan_image_json(SM_Context *ctx,const char *image_path,const char *languages,char **out_json);
SM_API const char *sm_last_error(SM_Context *ctx);
SM_API void sm_free_string(char *s);
#ifdef __cplusplus
}
#endif
#endif
