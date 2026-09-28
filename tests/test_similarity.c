#include "scanmatch.h"
#include <assert.h>
int main(void){assert(sm_hybrid_similarity("rice","rice")>.99);assert(sm_hybrid_similarity("rice","wheat")<.90);assert(sm_hybrid_similarity("ચોખા","ચોખા")>.99);return 0;}
