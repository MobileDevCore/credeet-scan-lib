#ifndef SCANMATCH_TEXT_H
#define SCANMATCH_TEXT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Gujarati-first text normalization utilities. The original OCR text must be retained by callers. */
size_t sm_normalize_text(const char *input, char *output, size_t capacity);
size_t sm_normalize_digits(const char *input, char *output, size_t capacity);
int sm_detect_language(const char *text, char *output, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
