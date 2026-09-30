#ifndef SCANMATCH_TEXT_H
#define SCANMATCH_TEXT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Gujarati-first text normalization and UTF-8 utilities. */
void sm_trim(char *s);
size_t sm_utf8_cp_len(const unsigned char *s);
unsigned sm_utf8_cp_decode(const unsigned char *s, size_t n);
size_t sm_normalize_text(const char *input, char *output, size_t capacity);
size_t sm_normalize_digits(const char *input, char *output, size_t capacity);
int sm_detect_language(const char *text, char *output, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif

