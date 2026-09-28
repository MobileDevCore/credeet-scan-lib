#include "scanmatch_ffi.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char**argv){
 if(argc<2){fprintf(stderr,"Usage: scanmatch <image> [catalog.csv] [languages]\n");return 2;}
 const char*image=argv[1];const char*catalog="data/products.csv";const char*langs="eng+hin+guj";
 if(argc>=3){if(strstr(argv[2],".csv"))catalog=argv[2];else langs=argv[2];} if(argc>=4)langs=argv[3];
 SM_Context*c=sm_create_context();if(!c)return 3;if(sm_load_catalog_csv_ffi(c,catalog)){fprintf(stderr,"%s\n",sm_last_error(c));sm_destroy_context(c);return 4;}
 char*j=NULL;int rc=sm_scan_image_json(c,image,langs,&j);if(rc){fprintf(stderr,"%s\n",sm_last_error(c));sm_destroy_context(c);return 5;}puts(j);sm_free_string(j);sm_destroy_context(c);return 0;
}
