#include "scanmatch.h"
#include "scanmatch_text.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strtok_r strtok_s
#endif

// Converts a UTF-8 string into an array of 32-bit Unicode codepoints.
// This ensures that Gujarati multi-byte characters (e.g. 'ચો' or 'ખા')
// are treated as individual characters rather than separate raw bytes
// during edit-distance calculation.
static size_t cp_array(const char *s, unsigned *a, size_t cap) {
    size_t n = 0;
    if (!s) return 0;
    for (size_t i = 0; s[i] && n < cap; ) {
        size_t k = sm_utf8_cp_len((const unsigned char *)s + i);
        a[n++] = sm_utf8_cp_decode((const unsigned char *)s + i, k);
        i += k;
    }
    return n;
}

// Computes Unicode Levenshtein edit similarity between two UTF-8 strings.
// Returns a normalized score between 0.0 (completely different) and 1.0 (identical).
double sm_utf8_similarity(const char *a, const char *b) {
    if (!a || !b) return 0.0;
    if (!*a && !*b) return 1.0;
    if (!*a || !*b) return 0.0;

    unsigned A[SM_MAX_TEXT];
    unsigned B[SM_MAX_TEXT];
    size_t na = cp_array(a, A, SM_MAX_TEXT);
    size_t nb = cp_array(b, B, SM_MAX_TEXT);

    // O(min(na, nb)) memory dynamic programming row
    size_t *prev = (size_t *)malloc((nb + 1) * sizeof(size_t));
    size_t *cur  = (size_t *)malloc((nb + 1) * sizeof(size_t));
    if (!prev || !cur) {
        free(prev);
        free(cur);
        return 0.0;
    }

    for (size_t j = 0; j <= nb; j++) prev[j] = j;

    for (size_t i = 1; i <= na; i++) {
        cur[0] = i;
        for (size_t j = 1; j <= nb; j++) {
            size_t del = prev[j] + 1;
            size_t ins = cur[j - 1] + 1;
            size_t sub = prev[j - 1] + (A[i - 1] != B[j - 1] ? 1 : 0);
            size_t min_op = del < ins ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
            cur[j] = min_op;
        }
        size_t *t = prev;
        prev = cur;
        cur = t;
    }

    size_t dist = prev[nb];
    size_t max_len = na > nb ? na : nb;
    free(prev);
    free(cur);

    return max_len > 0 ? 1.0 - ((double)dist / (double)max_len) : 1.0;
}

// Measures token/word overlap between two strings (fraction of tokens in A present in B).
double sm_token_overlap(const char *a, const char *b) {
    char A[SM_MAX_TEXT], B[SM_MAX_TEXT];
    sm_normalize_text(a, A, sizeof(A));
    sm_normalize_text(b, B, sizeof(B));

    int total = 0, common = 0;
    char *sa = NULL;
    for (char *x = strtok_r(A, " ", &sa); x; x = strtok_r(NULL, " ", &sa)) {
        total++;
        char BB[SM_MAX_TEXT];
        snprintf(BB, sizeof(BB), "%s", B);
        char *sb = NULL;
        for (char *y = strtok_r(BB, " ", &sb); y; y = strtok_r(NULL, " ", &sb)) {
            if (strcmp(x, y) == 0) {
                common++;
                break;
            }
        }
    }
    return total > 0 ? (double)common / (double)total : 0.0;
}

// Blends character-level Levenshtein similarity (65%) with token overlap (35%).
// If strings are strictly identical after normalization, returns 1.0 immediately.
double sm_hybrid_similarity(const char *a, const char *b) {
    char A[SM_MAX_TEXT], B[SM_MAX_TEXT];
    sm_normalize_text(a, A, sizeof(A));
    sm_normalize_text(b, B, sizeof(B));
    if (strcmp(A, B) == 0) return 1.0;

    double char_sim = sm_utf8_similarity(A, B);
    double tok_sim  = sm_token_overlap(A, B);
    return 0.65 * char_sim + 0.35 * tok_sim;
}

// Maps numeric score to standard confidence level bands
const char *sm_confidence_band(double s) {
    if (s >= 0.90) return "HIGH";
    if (s >= 0.70) return "MEDIUM";
    return "LOW";
}

// Splits a CSV line taking quoted fields into account
static int split_csv(char *line, char **fields, int max) {
    int n = 0, in = 0;
    fields[n++] = line;
    for (char *p = line; *p; p++) {
        if (*p == '"') {
            if (in && p[1] == '"') p++; // escaped quote ""
            else in = !in;
        } else if (*p == ',' && !in) {
            *p = '\0';
            if (n < max) fields[n++] = p + 1;
        }
    }
    return n;
}

// Strips wrapping quotes and whitespace from a CSV field
static void unquote(char *s) {
    sm_trim(s);
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        s[n - 1] = '\0';
        memmove(s, s + 1, n - 1);
        sm_trim(s);
    }
}

// Adds pipe-delimited alias entries into destination list
static void add_list(char dst[][SM_MAX_ALIAS], size_t *cnt, size_t max, const char *s) {
    if (!s || !*s) return;
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", s);
    char *save = NULL;
    for (char *x = strtok_r(tmp, "|;\r\n", &save); x && *cnt < max; x = strtok_r(NULL, "|;\r\n", &save)) {
        sm_trim(x);
        if (*x) {
            snprintf(dst[(*cnt)++], SM_MAX_ALIAS, "%s", x);
        }
    }
}

// Appends valid units to product definition
static void add_unit(SM_Product *p, const char *s) {
    if (!s || !p || p->unit_count >= 16) return;
    snprintf(p->units[p->unit_count++], 32, "%s", s);
}

// Loads catalog products from CSV file into caller-provided SM_Product array
int sm_load_catalog_csv(const char *path, SM_Product *p, size_t max, size_t *out) {
    if (!path || !p || !out) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -2;

    char line[16384];
    size_t n = 0;
    int first = 1;

    while (n < max && fgets(line, sizeof(line), f)) {
        if (first) {
            first = 0;
            // Skip CSV header line
            if (strstr(line, "product_id") || strstr(line, "sku")) continue;
        }
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        char *F[12] = {0};
        int nf = split_csv(line, F, 12);
        if (nf < 4) continue;

        SM_Product *q = &p[n];
        memset(q, 0, sizeof(*q));
        for (int i = 0; i < nf; i++) unquote(F[i]);

        snprintf(q->sku, sizeof(q->sku), "%s", F[0]);
        snprintf(q->canonical_name, sizeof(q->canonical_name), "%s", F[1]);

        if (nf >= 10) {
            snprintf(q->brand, sizeof(q->brand), "%s", F[2]);
            snprintf(q->category, sizeof(q->category), "%s", F[3]);
            // Aliases: English (F[4]), Gujarati (F[5]), Hindi (F[6])
            add_list(q->aliases, &q->alias_count, SM_MAX_ALIASES, F[4]);
            add_list(q->aliases, &q->alias_count, SM_MAX_ALIASES, F[5]);
            add_list(q->aliases, &q->alias_count, SM_MAX_ALIASES, F[6]);
            // Common OCR errors (F[7])
            add_list(q->ocr_errors, &q->ocr_error_count, SM_MAX_ERRORS, F[7]);
            // Units and pack sizes (F[8], F[9])
            char tmp[2048];
            snprintf(tmp, sizeof(tmp), "%s|%s", F[8], F[9]);
            char *sv = NULL;
            for (char *x = strtok_r(tmp, "|;\r\n", &sv); x && q->unit_count < 16; x = strtok_r(NULL, "|;\r\n", &sv)) {
                add_unit(q, x);
            }
        } else {
            snprintf(q->category, sizeof(q->category), "%s", F[2]);
            add_list(q->aliases, &q->alias_count, SM_MAX_ALIASES, F[3]);
        }
        n++;
    }
    fclose(f);
    *out = n;
    return 0;
}

// Finds highest similarity among canonical name, language aliases, and known OCR error variations
static double best_alias(const char *text, const SM_Product *p, double *exact, double *brand) {
    double best = sm_hybrid_similarity(text, p->canonical_name);
    *exact = (best >= 0.999) ? 1.0 : 0.0;

    for (size_t j = 0; j < p->alias_count; j++) {
        double s = sm_hybrid_similarity(text, p->aliases[j]);
        if (s > best) best = s;
        if (s >= 0.999) *exact = 1.0;
    }

    for (size_t j = 0; j < p->ocr_error_count; j++) {
        double s = sm_hybrid_similarity(text, p->ocr_errors[j]);
        if (s > best) best = s;
    }

    *brand = p->brand[0] ? sm_hybrid_similarity(text, p->brand) : 0.0;
    return best;
}

// Core catalog matching function:
// Ranks catalog items against an extracted item using a hybrid weighted scoring model.
int sm_match_product_hybrid(const SM_ListItem *i, const SM_Product *p, size_t count,
                            SM_ProductMatch *out, size_t max, SM_EmbeddingFn ef, void *ud) {
    (void)ef; // Embedding function pointer kept for backwards compatibility but unused
    (void)ud;

    if (!i || !p || !out || max == 0) return -1;
    if (count > SM_MAX_PRODUCTS) count = SM_MAX_PRODUCTS;

    SM_ProductMatch *tmp = (SM_ProductMatch *)calloc(count, sizeof(*tmp));
    if (!tmp) return -2;

    for (size_t k = 0; k < count; k++) {
        double exact = 0.0, brand = 0.0;
        double best = best_alias(i->product_text, &p[k], &exact, &brand);
        double tok = sm_token_overlap(i->product_text, p[k].canonical_name);

        for (size_t j = 0; j < p[k].alias_count; j++) {
            double tok_alias = sm_token_overlap(i->product_text, p[k].aliases[j]);
            if (tok_alias > tok) tok = tok_alias;
        }

        // Check unit compatibility with catalog product
        double unit_score = 0.0;
        if (i->has_unit && i->unit[0] && p[k].unit_count > 0) {
            for (size_t j = 0; j < p[k].unit_count; j++) {
                if (sm_hybrid_similarity(i->unit, p[k].units[j]) > 0.99) {
                    unit_score = 1.0;
                    break;
                }
            }
        }

        double catalog = 0.45 + 0.35 * fmax(exact, fmax(best, tok)) + 0.20 * unit_score;
        double ocr = (i->ocr_confidence > 0.0) ? i->ocr_confidence : 0.70;

        // Weighted confidence score
        double score = 0.40 * best + 0.20 * tok + 0.18 * exact + 0.07 * brand + 0.10 * ocr + 0.05 * unit_score;
        if (exact > 0.99) score += 0.08;
        else if (best > 0.88) score += 0.03;
        if (score > 1.0) score = 1.0;

        snprintf(tmp[k].sku, sizeof(tmp[k].sku), "%s", p[k].sku);
        snprintf(tmp[k].canonical_name, sizeof(tmp[k].canonical_name), "%s", p[k].canonical_name);
        tmp[k].score = score;
        tmp[k].text_score = best;
        tmp[k].token_score = tok;
        tmp[k].exact_alias_score = exact;
        tmp[k].brand_score = brand;
        tmp[k].catalog_evidence = catalog;
        snprintf(tmp[k].confidence_band, sizeof(tmp[k].confidence_band), "%s", sm_confidence_band(score));
    }

    // Sort candidates descending by score
    for (size_t a = 0; a < count; a++) {
        for (size_t b = a + 1; b < count; b++) {
            if (tmp[b].score > tmp[a].score) {
                SM_ProductMatch t = tmp[a];
                tmp[a] = tmp[b];
                tmp[b] = t;
            }
        }
    }

    size_t n = count < max ? count : max;
    for (size_t k = 0; k < n; k++) {
        out[k] = tmp[k];
    }
    free(tmp);
    return (int)n;
}
