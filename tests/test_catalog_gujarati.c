#include "scanmatch.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void){SM_Product *p=calloc(SM_MAX_PRODUCTS,sizeof(SM_Product));assert(p);size_t n=0;assert(sm_load_catalog_csv("../data/products.csv",p,SM_MAX_PRODUCTS,&n)==0);assert(n>=10);SM_ListItem i;assert(sm_parse_line("ચોખા ૫ કિલો",&i)==0);SM_ProductMatch m[3];int k=sm_match_product_hybrid(&i,p,n,m,3,NULL,NULL);assert(k>0);assert(strcmp(m[0].sku,"RICE001")==0);assert(sm_parse_line("સાબુ ૪ નંગ",&i)==0);k=sm_match_product_hybrid(&i,p,n,m,3,NULL,NULL);assert(k>0);assert(strcmp(m[0].sku,"SOAP001")==0);free(p);return 0;}
