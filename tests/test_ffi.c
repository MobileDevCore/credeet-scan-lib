#include "scanmatch_ffi.h"
#include <assert.h>
int main(void){SM_Context*c=sm_create_context();assert(c);assert(sm_last_error(c));sm_destroy_context(c);return 0;}
