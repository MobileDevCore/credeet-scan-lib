#include "scanmatch.h"
#include "scanmatch_ffi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    SM_Context *ctx = sm_create_context();
    assert(ctx != NULL);

    int rc = sm_load_catalog_csv_ffi(ctx, "data/products.csv");
    if (rc != 0) {
        rc = sm_load_catalog_csv_ffi(ctx, "../data/products.csv");
    }
    assert(rc == 0);

    char *json = NULL;

    // Test 1: English - "Rice 2 kg"
    rc = sm_process_text_json(ctx, "Rice 2 kg", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"product_id\":\"RICE001\"") != NULL);
    assert(strstr(json, "\"quantity\":2.000") != NULL);
    assert(strstr(json, "\"unit\":\"kg\"") != NULL);
    assert(strstr(json, "\"status\":\"confirmed\"") != NULL);
    sm_free_string(json);
    printf("PASS: English (Rice 2 kg)\n");

    // Test 2: Gujarati - "ચોખા ૨ કિલો"
    rc = sm_process_text_json(ctx, "ચોખા ૨ કિલો", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"product_id\":\"RICE001\"") != NULL);
    assert(strstr(json, "\"quantity\":2.000") != NULL);
    assert(strstr(json, "\"unit\":\"kg\"") != NULL);
    assert(strstr(json, "\"status\":\"confirmed\"") != NULL);
    sm_free_string(json);
    printf("PASS: Gujarati (ચોખા ૨ કિલો)\n");

    // Test 3: Mixed - "5 kg ચોખા"
    rc = sm_process_text_json(ctx, "5 kg ચોખા", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"product_id\":\"RICE001\"") != NULL);
    assert(strstr(json, "\"quantity\":5.000") != NULL);
    assert(strstr(json, "\"unit\":\"kg\"") != NULL);
    assert(strstr(json, "\"status\":\"confirmed\"") != NULL);
    sm_free_string(json);
    printf("PASS: Mixed (5 kg ચોખા)\n");

    // Test 4: Hindi - "चावल 2 किलो"
    rc = sm_process_text_json(ctx, "चावल 2 किलो", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"product_id\":\"RICE001\"") != NULL);
    assert(strstr(json, "\"quantity\":2.000") != NULL);
    assert(strstr(json, "\"unit\":\"kg\"") != NULL);
    assert(strstr(json, "\"status\":\"confirmed\"") != NULL);
    sm_free_string(json);
    printf("PASS: Hindi (चावल 2 किलो)\n");

    // Test 5: Unknown product - "XYZRandomGadget 3 pcs"
    rc = sm_process_text_json(ctx, "XYZRandomGadget 3 pcs", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"product_id\":null") != NULL);
    assert(strstr(json, "\"status\":\"unidentified\"") != NULL);
    assert(strstr(json, "\"quantity\":3.000") != NULL);
    assert(strstr(json, "\"unit\":\"piece\"") != NULL);
    sm_free_string(json);
    printf("PASS: Unknown product\n");

    // Test 6: Ambiguous product - "Soap"
    // In catalog: SOAP001 (Soap) and LUX001 (Lux Soap)
    rc = sm_process_text_json(ctx, "Soap 1 piece", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"candidates\":[") != NULL);
    sm_free_string(json);
    printf("PASS: Ambiguous/candidate checking (Soap)\n");

    // Test 7: Missing quantity - "ચોખા"
    // Must NOT invent quantity!
    rc = sm_process_text_json(ctx, "ચોખા", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"product_id\":\"RICE001\"") != NULL);
    assert(strstr(json, "\"quantity\":null") != NULL);
    assert(strstr(json, "\"has_quantity\":false") != NULL);
    assert(strstr(json, "\"unit\":null") != NULL);
    assert(strstr(json, "\"has_unit\":false") != NULL);
    sm_free_string(json);
    printf("PASS: Missing quantity (no invented quantity)\n");

    // Test 8: Missing unit - "ચોખા ૨"
    // Must NOT invent unit!
    rc = sm_process_text_json(ctx, "ચોખા ૨", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"product_id\":\"RICE001\"") != NULL);
    assert(strstr(json, "\"quantity\":2.000") != NULL);
    assert(strstr(json, "\"has_quantity\":true") != NULL);
    assert(strstr(json, "\"unit\":null") != NULL);
    assert(strstr(json, "\"has_unit\":false") != NULL);
    sm_free_string(json);
    printf("PASS: Missing unit (no invented unit)\n");

    // Test 9: Invalid input - NULL pointer
    rc = sm_process_text_json(ctx, NULL, &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"code\":\"NO_TEXT_DETECTED\"") != NULL);
    sm_free_string(json);
    printf("PASS: Invalid input (NULL pointer handled)\n");

    // Test 10: No OCR text - whitespace only
    rc = sm_process_text_json(ctx, "   \n  \t  \r\n  ", &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "\"code\":\"NO_TEXT_DETECTED\"") != NULL);
    sm_free_string(json);
    printf("PASS: No OCR text (whitespace-only returns NO_TEXT_DETECTED)\n");

    // Test 11: Multiple items multiline
    const char *multiline = "ચોખા ૫ કિલો\nદૂધ ૧ લિટર\nસાબુ ૪ નંગ\nRice 2 kg\nचावल 2 किलो";
    rc = sm_process_text_json(ctx, multiline, &json);
    assert(rc == 0 && json != NULL);
    assert(strstr(json, "RICE001") != NULL);
    assert(strstr(json, "MILK001") != NULL);
    assert(strstr(json, "SOAP001") != NULL);
    sm_free_string(json);
    printf("PASS: Multiple items multiline processing\n");

    sm_destroy_context(ctx);
    printf("\nALL COMPREHENSIVE TESTS PASSED!\n");
    return 0;
}
