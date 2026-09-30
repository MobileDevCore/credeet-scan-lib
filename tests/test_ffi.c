#include "scanmatch_ffi.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    SM_Context *c = sm_create_context();
    assert(c != NULL);

    // Empty catalog check
    char *out_json = NULL;
    int rc = sm_process_text_json(c, "rice 2 kg", &out_json);
    assert(rc == -2); // CATALOG_EMPTY

    // Load catalog
    assert(sm_load_catalog_csv_ffi(c, "data/products.csv") == 0 ||
           sm_load_catalog_csv_ffi(c, "../data/products.csv") == 0);

    // Process text
    rc = sm_process_text_json(c, "rice 2 kg", &out_json);
    assert(rc == 0);
    assert(out_json != NULL);
    assert(strstr(out_json, "RICE001") != NULL);
    assert(strstr(out_json, "\"has_quantity\":true") != NULL);
    sm_free_string(out_json);

    sm_destroy_context(c);
    return 0;
}
