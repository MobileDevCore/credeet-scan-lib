#include "scanmatch_text.h"
#include "scanmatch.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// In-place whitespace trimmer: strips leading and trailing ASCII whitespace.
void sm_trim(char *s) {
    if (!s || !*s) return;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) {
        i++;
    }
    if (i > 0) {
        memmove(s, s + i, strlen(s + i) + 1);
    }
}

// Determines the byte length of a UTF-8 character based on its leading byte.
size_t sm_utf8_cp_len(const unsigned char *s) {
    if (!s || !*s) return 0;
    if (*s < 0x80) return 1;
    if ((*s & 0xE0) == 0xC0) return 2;
    if ((*s & 0xF0) == 0xE0) return 3;
    if ((*s & 0xF8) == 0xF0) return 4;
    return 1;
}

// Decodes a multibyte UTF-8 sequence (1 to 4 bytes) into its Unicode codepoint value.
unsigned sm_utf8_cp_decode(const unsigned char *s, size_t n) {
    if (!s || !n) return 0xFFFD;
    if (n == 1) return s[0];
    if (n == 2) return ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    if (n == 3) return ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    return ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) | ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

// Checks if a codepoint represents ASCII or non-breaking whitespace.
static int is_space_cp(unsigned cp) {
    return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n' || cp == 0x00A0;
}

// Checks if a codepoint is standard ASCII punctuation.
static int is_punct_cp(unsigned cp) {
    return cp < 128 && ispunct((unsigned char)cp);
}

// Map Gujarati digits (U+0AE6 '૦' to U+0AEF '૯') to integer 0..9.
static int guj_digit(unsigned cp) {
    return (cp >= 0x0AE6 && cp <= 0x0AEF) ? (int)(cp - 0x0AE6) : -1;
}

// Map Devanagari digits (U+0966 '०' to U+096F '९') to integer 0..9.
static int dev_digit(unsigned cp) {
    return (cp >= 0x0966 && cp <= 0x096F) ? (int)(cp - 0x0966) : -1;
}

// Emits an ASCII byte into the destination buffer if space permits.
static void emit_ascii(char *out, size_t cap, size_t *j, char c) {
    if (*j + 1 < cap) {
        out[(*j)++] = c;
    }
}

// Convert Gujarati (૦-૯) and Devanagari (०-९) digits to standard ASCII digits ('0'-'9')
// so the quantity/unit parser can process mixed numerals like "૫ kg" and "5 kg" identically.
size_t sm_normalize_digits(const char *input, char *output, size_t capacity) {
    if (!output || capacity == 0) return 0;
    size_t j = 0;
    if (!input) {
        output[0] = '\0';
        return 0;
    }

    for (size_t i = 0; input[i]; ) {
        size_t n = sm_utf8_cp_len((const unsigned char *)input + i);
        unsigned cp = sm_utf8_cp_decode((const unsigned char *)input + i, n);
        if (cp == 0xFEFF) { // Skip UTF-8 Byte Order Mark
            i += n;
            continue;
        }
        int d = guj_digit(cp);
        if (d < 0) d = dev_digit(cp);

        if (d >= 0) {
            emit_ascii(output, capacity, &j, (char)('0' + d));
        } else {
            if (j + n < capacity) {
                memcpy(output + j, input + i, n);
                j += n;
            } else {
                break;
            }
        }
        i += n;
    }
    output[j] = '\0';
    return j;
}

// Normalizes mixed Gujarati/Hindi/English text by lowercasing Latin characters,
// replacing Gujarati/Devanagari digits with ASCII digits, collapsing whitespace,
// and stripping unnecessary punctuation while preserving Indic Unicode letters.
size_t sm_normalize_text(const char *input, char *output, size_t capacity) {
    if (!output || capacity == 0) return 0;
    output[0] = '\0';
    if (!input) return 0;

    size_t j = 0;
    int space = 1; // start in space state to suppress leading whitespace
    char digitbuf[SM_MAX_TEXT * 2];
    sm_normalize_digits(input, digitbuf, sizeof(digitbuf));

    for (size_t i = 0; digitbuf[i]; ) {
        size_t n = sm_utf8_cp_len((const unsigned char *)digitbuf + i);
        unsigned cp = sm_utf8_cp_decode((const unsigned char *)digitbuf + i, n);
        if (cp == 0xFEFF) { // Skip UTF-8 Byte Order Mark
            i += n;
            continue;
        }

        if (cp < 128) {
            if (isalnum((unsigned char)cp)) {
                emit_ascii(output, capacity, &j, (char)tolower((unsigned char)cp));
                space = 0;
            } else if (!space) {
                emit_ascii(output, capacity, &j, ' ');
                space = 1;
            }
        } else if (is_space_cp(cp) || is_punct_cp(cp)) {
            if (!space) {
                emit_ascii(output, capacity, &j, ' ');
                space = 1;
            }
        } else {
            // Indic characters (Gujarati U+0A80..U+0AFF, Devanagari U+0900..U+097F)
            if (j + n < capacity) {
                memcpy(output + j, digitbuf + i, n);
                j += n;
                space = 0;
            } else {
                break;
            }
        }
        i += n;
    }
    // Remove any trailing space
    if (j && output[j - 1] == ' ') {
        j--;
    }
    output[j] = '\0';
    return j;
}

// Detects primary script by counting character frequencies:
// Gujarati (U+0A80..U+0AFF), Devanagari (U+0900..U+097F), Latin ASCII letters.
int sm_detect_language(const char *text, char *output, size_t capacity) {
    if (!output || capacity == 0) return -1;
    size_t guj = 0, dev = 0, latin = 0;
    if (text) {
        for (size_t i = 0; text[i]; ) {
            size_t n = sm_utf8_cp_len((const unsigned char *)text + i);
            unsigned cp = sm_utf8_cp_decode((const unsigned char *)text + i, n);
            if (cp >= 0x0A80 && cp <= 0x0AFF) guj++;
            else if (cp >= 0x0900 && cp <= 0x097F) dev++;
            else if (cp < 128 && isalpha((unsigned char)cp)) latin++;
            i += n;
        }
    }
    const char *r = (guj >= dev && guj > 0) ? "guj" : (dev > 0 ? "hin" : (latin > 0 ? "eng" : "unknown"));
    snprintf(output, capacity, "%s", r);
    return 0;
}
