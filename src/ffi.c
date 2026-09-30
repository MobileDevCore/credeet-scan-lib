#include "scanmatch.h"
#include "scanmatch_ffi.h"
#include "scanmatch_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef _WIN32
#define strtok_r strtok_s
#endif

// Internal library context holding loaded catalog and error status
struct SM_Context {
    SM_Product products[SM_MAX_PRODUCTS];
    size_t product_count;
    char error[512];
};

static void set_error(SM_Context *c, const char *fmt, ...) {
    if (!c) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(c->error, sizeof(c->error), fmt, args);
    va_end(args);
}

SM_Context *sm_create_context(void) {
    return (SM_Context *)calloc(1, sizeof(SM_Context));
}

void sm_destroy_context(SM_Context *ctx) {
    if (ctx) {
        free(ctx);
    }
}

int sm_load_catalog_csv_ffi(SM_Context *ctx, const char *path) {
    if (!ctx || !path) return -1;
    int rc = sm_load_catalog_csv(path, ctx->products, SM_MAX_PRODUCTS, &ctx->product_count);
    if (rc != 0) {
        set_error(ctx, "CATALOG_LOAD_FAILED:%d", rc);
    } else if (ctx->product_count == 0) {
        set_error(ctx, "CATALOG_EMPTY");
        rc = -2;
    }
    return rc;
}

const char *sm_last_error(SM_Context *ctx) {
    return ctx ? ctx->error : "INVALID_CONTEXT";
}

void sm_free_string(char *s) {
    if (s) {
        free(s);
    }
}

// Appends formatted text to dynamically growing heap buffer with safety cap (4 MB max)
static int append_fmt(char **buf, size_t *cap, size_t *len, const char *fmt, ...) {
    for (;;) {
        va_list args;
        va_start(args, fmt);
        int needed = vsnprintf(*buf + *len, *cap - *len, fmt, args);
        va_end(args);

        if (needed < 0) return -1;
        if (*len + (size_t)needed < *cap) {
            *len += (size_t)needed;
            return 0;
        }

        size_t new_cap = *cap * 2;
        if (new_cap < *len + (size_t)needed + 1) {
            new_cap = *len + (size_t)needed + 1;
        }
        if (new_cap > 4 * 1024 * 1024) return -2; // Safety cap against unbounded growth

        char *new_buf = (char *)realloc(*buf, new_cap);
        if (!new_buf) return -3;
        *buf = new_buf;
        *cap = new_cap;
    }
}

// Appends a JSON-escaped string with enclosing quotes
static int append_json_str(char **buf, size_t *cap, size_t *len, const char *s) {
    if (append_fmt(buf, cap, len, "\"")) return -1;
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        if (*p == '"' || *p == '\\') {
            if (append_fmt(buf, cap, len, "\\%c", *p)) return -1;
        } else if (*p == '\n') {
            if (append_fmt(buf, cap, len, "\\n")) return -1;
        } else if (*p == '\r') {
            if (append_fmt(buf, cap, len, "\\r")) return -1;
        } else if (*p == '\t') {
            if (append_fmt(buf, cap, len, "\\t")) return -1;
        } else if (*p < 32) {
            if (append_fmt(buf, cap, len, " ")) return -1;
        } else {
            if (append_fmt(buf, cap, len, "%c", *p)) return -1;
        }
    }
    return append_fmt(buf, cap, len, "\"");
}

// Core FFI entry point: Processes raw OCR text from Python, normalizes Gujarati/Hindi/English,
// extracts quantities and units, matches against loaded catalog, and generates structured JSON.
int sm_process_text_json(SM_Context *ctx, const char *ocr_text, char **out_json) {
    if (!ctx || !out_json) return -1;
    *out_json = NULL;

    if (ctx->product_count == 0) {
        set_error(ctx, "CATALOG_EMPTY");
        return -2;
    }

    size_t cap = 4096;
    size_t len = 0;
    char *json = (char *)malloc(cap);
    if (!json) {
        set_error(ctx, "OUT_OF_MEMORY");
        return -3;
    }

    // Check for missing or empty OCR text
    int has_content = 0;
    if (ocr_text) {
        for (const char *p = ocr_text; *p; p++) {
            if (!isspace((unsigned char)*p)) {
                has_content = 1;
                break;
            }
        }
    }

    if (!has_content) {
        append_fmt(&json, &cap, &len,
            "{\"success\":false,\"error\":{\"code\":\"NO_TEXT_DETECTED\","
            "\"message\":\"No readable text was detected in the OCR input.\"},"
            "\"items\":[]}");
        *out_json = json;
        return 0;
    }

    char detected_lang[16] = {0};
    sm_detect_language(ocr_text, detected_lang, sizeof(detected_lang));

    append_fmt(&json, &cap, &len, "{\"success\":true,\"language\":");
    append_json_str(&json, &cap, &len, detected_lang);
    append_fmt(&json, &cap, &len, ",\"items\":[");

    // Make local copy of text for thread-safe line splitting
    size_t text_len = strlen(ocr_text);
    char *copy = (char *)malloc(text_len + 1);
    if (!copy) {
        free(json);
        set_error(ctx, "OUT_OF_MEMORY");
        return -3;
    }
    memcpy(copy, ocr_text, text_len + 1);

    char *saveptr = NULL;
    int item_count = 0;

    for (char *line = strtok_r(copy, "\r\n", &saveptr); line; line = strtok_r(NULL, "\r\n", &saveptr)) {
        sm_trim(line);
        if (!line[0]) continue;

        char normalized[SM_MAX_TEXT];
        sm_normalize_text(line, normalized, sizeof(normalized));

        SM_ListItem item;
        if (sm_parse_line(line, &item) != 0 || !item.product_text[0]) {
            continue;
        }

        SM_ProductMatch matches[SM_MAX_CANDIDATES];
        int num_matches = sm_match_product_hybrid(&item, ctx->products, ctx->product_count,
                                                  matches, SM_MAX_CANDIDATES, NULL, NULL);

        if (item_count > 0) {
            append_fmt(&json, &cap, &len, ",");
        }
        item_count++;

        append_fmt(&json, &cap, &len, "{\"raw_text\":");
        append_json_str(&json, &cap, &len, line);
        append_fmt(&json, &cap, &len, ",\"normalized_text\":");
        append_json_str(&json, &cap, &len, normalized);
        append_fmt(&json, &cap, &len, ",\"product_text\":");
        append_json_str(&json, &cap, &len, item.product_text);

        // Strict quantity handling: Do NOT invent a quantity if absent
        if (item.has_quantity) {
            append_fmt(&json, &cap, &len, ",\"quantity\":%.3f,\"has_quantity\":true", item.quantity);
        } else {
            append_fmt(&json, &cap, &len, ",\"quantity\":null,\"has_quantity\":false");
        }

        // Strict unit handling: Do NOT invent a unit if absent
        if (item.has_unit) {
            append_fmt(&json, &cap, &len, ",\"unit\":");
            append_json_str(&json, &cap, &len, item.unit);
            append_fmt(&json, &cap, &len, ",\"has_unit\":true");
        } else {
            append_fmt(&json, &cap, &len, ",\"unit\":null,\"has_unit\":false");
        }

        // Conservative matching: Check for unidentified and ambiguous candidates
        if (num_matches <= 0 || matches[0].score < 0.55) {
            append_fmt(&json, &cap, &len,
                ",\"product_id\":null,\"name\":null,\"confidence\":0.0,"
                "\"status\":\"unidentified\",\"needs_confirmation\":true,"
                "\"reason\":\"insufficient_confidence\",\"candidates\":[]}");
            continue;
        }

        int is_ambiguous = (num_matches > 1 && (matches[0].score - matches[1].score) < 0.08 && matches[0].score < 0.95);
        int is_confirmed = (matches[0].score >= 0.90 && !is_ambiguous);

        const char *status = is_ambiguous ? "ambiguous" : (is_confirmed ? "confirmed" : "needs_confirmation");
        const char *reason = is_ambiguous ? "multiple_close_candidates" :
                             (is_confirmed ? "high_confidence_match" : "moderate_confidence");

        append_fmt(&json, &cap, &len, ",\"product_id\":");
        append_json_str(&json, &cap, &len, matches[0].sku);
        append_fmt(&json, &cap, &len, ",\"name\":");
        append_json_str(&json, &cap, &len, matches[0].canonical_name);
        append_fmt(&json, &cap, &len, ",\"confidence\":%.4f,\"status\":\"%s\",\"needs_confirmation\":%s,\"reason\":\"%s\",\"candidates\":[",
                   matches[0].score, status, (is_confirmed ? "false" : "true"), reason);

        for (int k = 0; k < num_matches; k++) {
            if (k > 0) append_fmt(&json, &cap, &len, ",");
            append_fmt(&json, &cap, &len, "{\"product_id\":");
            append_json_str(&json, &cap, &len, matches[k].sku);
            append_fmt(&json, &cap, &len, ",\"name\":");
            append_json_str(&json, &cap, &len, matches[k].canonical_name);
            append_fmt(&json, &cap, &len, ",\"score\":%.4f,\"confidence\":\"%s\"}",
                       matches[k].score, matches[k].confidence_band);
        }
        append_fmt(&json, &cap, &len, "]}");
    }

    free(copy);
    append_fmt(&json, &cap, &len, "]}");

    *out_json = json;
    return 0;
}
