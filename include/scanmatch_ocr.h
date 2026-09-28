#ifndef SCANMATCH_OCR_H
#define SCANMATCH_OCR_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const char *languages; int psm; int preprocess; int scale; const char *tessdata_path; } SM_OCR_Config;
typedef struct {
    char *text;
    double mean_confidence;
    int rotation_degrees;
    int width;
    int height;
    double quality_score;
    char quality_reason[128];
    long long orientation_detection_ms;
    long long preprocessing_ms;
    long long ocr_ms;
    long long fallback_ocr_ms;
    long long total_ocr_pipeline_ms;
    long long ocr_time_ms;
    int ocr_passes;
} SM_OCR_Result;
int sm_ocr_image_config(const char*, const SM_OCR_Config*, SM_OCR_Result*);
void sm_ocr_result_free(SM_OCR_Result*);
#ifdef __cplusplus
}
#endif
#endif
