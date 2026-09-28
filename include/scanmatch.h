#ifndef SCANMATCH_H
#define SCANMATCH_H
#include <stddef.h>
#include "scanmatch_text.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SM_MAX_TEXT 512
#define SM_MAX_ALIAS 256
#define SM_MAX_SKU 64
#define SM_MAX_CATEGORY 64
#define SM_MAX_BRAND 64
#define SM_MAX_PRODUCTS 4096
#define SM_MAX_CANDIDATES 5
#define SM_MAX_ALIASES 32
#define SM_MAX_ERRORS 32

typedef struct {
    char product_text[SM_MAX_TEXT];
    double quantity;
    char unit[32];
    char language[16];
    double ocr_confidence;
} SM_ListItem;

typedef struct {
    char sku[SM_MAX_SKU];
    char canonical_name[SM_MAX_TEXT];
    char brand[SM_MAX_BRAND];
    char category[SM_MAX_CATEGORY];
    char aliases[SM_MAX_ALIASES][SM_MAX_ALIAS];
    size_t alias_count;
    char ocr_errors[SM_MAX_ERRORS][SM_MAX_ALIAS];
    size_t ocr_error_count;
    char units[16][32];
    size_t unit_count;
    char pack_sizes[16][32];
    size_t pack_size_count;
} SM_Product;

typedef struct {
    char sku[SM_MAX_SKU];
    char canonical_name[SM_MAX_TEXT];
    double score;
    double text_score;
    double token_score;
    double embedding_score;
    double exact_alias_score;
    double brand_score;
    double catalog_evidence;
    char confidence_band[16];
} SM_ProductMatch;

typedef int (*SM_EmbeddingFn)(const char*, float*, size_t, void*);
double sm_utf8_similarity(const char*, const char*);
double sm_token_overlap(const char*, const char*);
double sm_hybrid_similarity(const char*, const char*);
const char *sm_confidence_band(double);
int sm_parse_line(const char*, SM_ListItem*);
int sm_load_catalog_csv(const char*, SM_Product*, size_t, size_t*);
int sm_match_product_hybrid(const SM_ListItem*, const SM_Product*, size_t, SM_ProductMatch*, size_t, SM_EmbeddingFn, void*);
double sm_cosine_similarity(const float*, const float*, size_t);
double sm_embedding_blend(double,double);
#ifdef __cplusplus
}
#endif
#endif
