#include "scanmatch.h"
#include <stdio.h>

int main(void) {
    SM_ListItem item;
    if (sm_parse_line("2 kg rice", &item) != 0) return 1;
    printf("item=%s quantity=%.2f unit=%s\n",
           item.product_text,item.quantity,item.unit);

    printf("similarity rice/rice = %.3f\n",
           sm_hybrid_similarity("rice","rice"));
    return 0;
}
