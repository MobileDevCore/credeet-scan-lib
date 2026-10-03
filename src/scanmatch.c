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

/* Stack allocation threshold for Unicode Levenshtein edit distance.
 * Typical grocery product tokens are well under 64 codepoints.
 * A 256-element stack buffer (256 * sizeof(size_t) = 2 KB) eliminates
 * all heap malloc/free calls in inner matching loops while remaining
 * fully thread-safe (stack-local per thread) and bounded against overflow.
 * If input exceeds 256 codepoints, a safe dynamic heap fallback is used.
 */
#define SM_STACK_ROW_CAP 256

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

double sm_utf8_similarity(const char *a, const char *b) {
    if (!a || !b) return 0.0;
    if (!*a && !*b) return 1.0;
    if (!*a || !*b) return 0.0;
    if (strcmp(a, b) == 0) return 1.0;

    unsigned A[SM_MAX_TEXT];
    unsigned B[SM_MAX_TEXT];
    size_t na = cp_array(a, A, SM_MAX_TEXT);
    size_t nb = cp_array(b, B, SM_MAX_TEXT);

    if (na == 0 && nb == 0) return 1.0;
    if (na == 0 || nb == 0) return 0.0;

    size_t max_len = na > nb ? na : nb;
    size_t min_len = na < nb ? na : nb;
    size_t len_diff = max_len - min_len;

    /* Early filter: theoretical maximum similarity based purely on length delta */
    double max_possible = 1.0 - ((double)len_diff / (double)max_len);
    if (max_possible < 0.20) {
        return max_possible;
    }

    /* Ensure B is the shorter sequence to minimize DP row width */
    const unsigned *pA = A;
    const unsigned *pB = B;
    if (nb > na) {
        pA = B;
        pB = A;
        size_t tmp = na;
        na = nb;
        nb = tmp;
    }

    size_t stack_prev[SM_STACK_ROW_CAP + 1];
    size_t stack_cur[SM_STACK_ROW_CAP + 1];
    size_t *prev = stack_prev;
    size_t *cur = stack_cur;
    int allocated = 0;

    if (nb > SM_STACK_ROW_CAP) {
        prev = (size_t *)malloc((nb + 1) * sizeof(size_t));
        cur  = (size_t *)malloc((nb + 1) * sizeof(size_t));
        if (!prev || !cur) {
            free(prev);
            free(cur);
            return max_possible;
        }
        allocated = 1;
    }

    for (size_t j = 0; j <= nb; j++) prev[j] = j;

    for (size_t i = 1; i <= na; i++) {
        cur[0] = i;
        unsigned a_val = pA[i - 1];
        for (size_t j = 1; j <= nb; j++) {
            size_t del = prev[j] + 1;
            size_t ins = cur[j - 1] + 1;
            size_t sub = prev[j - 1] + (a_val != pB[j - 1] ? 1 : 0);
            size_t min_op = del < ins ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
            cur[j] = min_op;
        }
        size_t *t = prev;
        prev = cur;
        cur = t;
    }

    size_t dist = prev[nb];
    if (allocated) {
        free(prev);
        free(cur);
    }

    return max_len > 0 ? 1.0 - ((double)dist / (double)max_len) : 1.0;
}

/* Fast token overlap on pre-normalized strings to prevent redundant normalization */
static double token_overlap_fast(const char *A, const char *B) {
    if (!A || !B || !*A || !*B) return 0.0;
    if (strcmp(A, B) == 0) return 1.0;

    char copyA[SM_MAX_TEXT];
    snprintf(copyA, sizeof(copyA), "%s", A);

    int total = 0, common = 0;
    char *sa = NULL;
    for (char *x = strtok_r(copyA, " ", &sa); x; x = strtok_r(NULL, " ", &sa)) {
        total++;
        char copyB[SM_MAX_TEXT];
        snprintf(copyB, sizeof(copyB), "%s", B);
        char *sb = NULL;
        for (char *y = strtok_r(copyB, " ", &sb); y; y = strtok_r(NULL, " ", &sb)) {
            if (strcmp(x, y) == 0) {
                common++;
                break;
            }
        }
    }
    return total > 0 ? (double)common / (double)total : 0.0;
}

double sm_token_overlap(const char *a, const char *b) {
    char A[SM_MAX_TEXT], B[SM_MAX_TEXT];
    sm_normalize_text(a, A, sizeof(A));
    sm_normalize_text(b, B, sizeof(B));
    return token_overlap_fast(A, B);
}

double sm_hybrid_similarity(const char *a, const char *b) {
    if (!a || !b) return 0.0;
    char A[SM_MAX_TEXT], B[SM_MAX_TEXT];
    sm_normalize_text(a, A, sizeof(A));
    sm_normalize_text(b, B, sizeof(B));
    if (strcmp(A, B) == 0) return 1.0;

    double char_sim = sm_utf8_similarity(A, B);
    double tok_sim  = token_overlap_fast(A, B);
    return 0.65 * char_sim + 0.35 * tok_sim;
}

const char *sm_confidence_band(double s) {
    if (s >= 0.80) return "HIGH";
    if (s >= 0.50) return "MEDIUM";
    return "LOW";
}

static int split_csv(char *line, char **fields, int max) {
    int n = 0, in = 0;
    fields[n++] = line;
    for (char *p = line; *p; p++) {
        if (*p == '"') {
            if (in && p[1] == '"') p++;
            else in = !in;
        } else if (*p == ',' && !in) {
            *p = '\0';
            if (n < max) fields[n++] = p + 1;
        }
    }
    return n;
}

static void unquote(char *s) {
    sm_trim(s);
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        s[n - 1] = '\0';
        memmove(s, s + 1, n - 1);
        sm_trim(s);
    }
}

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

static void add_unit(SM_Product *p, const char *s) {
    if (!s || !p || p->unit_count >= 16) return;
    snprintf(p->units[p->unit_count++], 32, "%s", s);
}

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
            add_list(q->aliases, &q->alias_count, SM_MAX_ALIASES, F[4]);
            add_list(q->aliases, &q->alias_count, SM_MAX_ALIASES, F[5]);
            add_list(q->aliases, &q->alias_count, SM_MAX_ALIASES, F[6]);
            add_list(q->ocr_errors, &q->ocr_error_count, SM_MAX_ERRORS, F[7]);

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

/* Fast candidate scoring against normalized query text */
static double score_product(const char *norm_query, size_t query_len, const SM_Product *p, double *exact_out, double *brand_out) {
    char norm_canon[SM_MAX_TEXT];
    sm_normalize_text(p->canonical_name, norm_canon, sizeof(norm_canon));

    if (strcmp(norm_query, norm_canon) == 0) {
        *exact_out = 1.0;
        *brand_out = 0.0;
        return 1.0;
    }

    double best = 0.0;
    double exact = 0.0;

    /* 1. Fast exact check across all aliases first */
    for (size_t j = 0; j < p->alias_count; j++) {
        char norm_alias[SM_MAX_TEXT];
        sm_normalize_text(p->aliases[j], norm_alias, sizeof(norm_alias));
        if (strcmp(norm_query, norm_alias) == 0) {
            *exact_out = 1.0;
            *brand_out = (p->brand[0] && strstr(norm_query, p->brand)) ? 1.0 : 0.0;
            return 1.0;
        }
    }

    /* 2. Fuzzy match canonical name */
    best = sm_hybrid_similarity(norm_query, norm_canon);

    /* 3. Fuzzy match aliases with length-based early filtering */
    for (size_t j = 0; j < p->alias_count; j++) {
        char norm_alias[SM_MAX_TEXT];
        sm_normalize_text(p->aliases[j], norm_alias, sizeof(norm_alias));
        size_t a_len = strlen(norm_alias);
        if (a_len == 0) continue;

        /* Skip expensive comparison if length difference makes a high score impossible */
        if (query_len > a_len * 3 || a_len > query_len * 3) continue;

        double s = sm_hybrid_similarity(norm_query, norm_alias);
        if (s > best) best = s;
        if (s >= 0.999) {
            exact = 1.0;
            break;
        }
    }

    /* 4. Check known OCR error patterns */
    for (size_t j = 0; j < p->ocr_error_count; j++) {
        double s = sm_hybrid_similarity(norm_query, p->ocr_errors[j]);
        if (s > best) best = s;
    }

    /* 5. Brand score */
    double brand = 0.0;
    if (p->brand[0]) {
        char norm_brand[SM_MAX_TEXT];
        sm_normalize_text(p->brand, norm_brand, sizeof(norm_brand));
        brand = sm_hybrid_similarity(norm_query, norm_brand);
    }

    *exact_out = exact;
    *brand_out = brand;
    return best;
}

int sm_match_product_hybrid(const SM_ListItem *i, const SM_Product *p, size_t count,
                            SM_ProductMatch *out, size_t max, SM_EmbeddingFn ef, void *ud) {
    (void)ef;
    (void)ud;

    if (!i || !p || !out || max == 0) return -1;
    if (count > SM_MAX_PRODUCTS) count = SM_MAX_PRODUCTS;

    /* Normalize input query text once per input line */
    char norm_query[SM_MAX_TEXT];
    sm_normalize_text(i->product_text, norm_query, sizeof(norm_query));
    size_t q_len = strlen(norm_query);

    SM_ProductMatch *tmp = (SM_ProductMatch *)calloc(count, sizeof(*tmp));
    if (!tmp) return -2;

    for (size_t k = 0; k < count; k++) {
        double exact = 0.0, brand = 0.0;
        double best = score_product(norm_query, q_len, &p[k], &exact, &brand);
        double tok = sm_token_overlap(norm_query, p[k].canonical_name);

        for (size_t j = 0; j < p[k].alias_count; j++) {
            double tok_alias = sm_token_overlap(norm_query, p[k].aliases[j]);
            if (tok_alias > tok) tok = tok_alias;
        }

        double unit_score = 0.0;
        if (i->has_unit && i->unit[0] && p[k].unit_count > 0) {
            for (size_t j = 0; j < p[k].unit_count; j++) {
                if (sm_hybrid_similarity(i->unit, p[k].units[j]) > 0.95) {
                    unit_score = 1.0;
                    break;
                }
            }
        }

        double catalog = 0.45 + 0.35 * fmax(exact, fmax(best, tok)) + 0.20 * unit_score;
        double ocr = (i->ocr_confidence > 0.0) ? i->ocr_confidence : 0.70;

        /* Weighted confidence score with exact match reinforcement */
        double score = 0.42 * best + 0.22 * tok + 0.16 * exact + 0.05 * brand + 0.10 * ocr + 0.05 * unit_score;
        if (exact > 0.99) score += 0.10;
        else if (best > 0.88) score += 0.04;
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

    /* Sort candidates descending by score */
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
