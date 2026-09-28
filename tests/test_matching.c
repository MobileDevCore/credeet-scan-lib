#include "scanmatch.h"
#include <assert.h>
#include <string.h>
int main(void){
 SM_Product p[3];memset(p,0,sizeof(p));strcpy(p[0].sku,"R");strcpy(p[0].canonical_name,"Rice");strcpy(p[0].aliases[0],"ચોખા");p[0].alias_count=1;
 strcpy(p[1].sku,"S");strcpy(p[1].canonical_name,"Soap");strcpy(p[1].aliases[0],"સાબુ");p[1].alias_count=1;
 strcpy(p[2].sku,"C");strcpy(p[2].canonical_name,"Clinic Plus Shampoo");strcpy(p[2].brand,"Clinic Plus");strcpy(p[2].aliases[0],"clinic plus");strcpy(p[2].aliases[1],"clinic plus shampoo");p[2].alias_count=2;
 SM_ListItem i;memset(&i,0,sizeof(i));strcpy(i.product_text,"ચોખા");i.ocr_confidence=.95;SM_ProductMatch m[3];int n=sm_match_product_hybrid(&i,p,3,m,3,NULL,NULL);assert(n==3);assert(strcmp(m[0].sku,"R")==0);assert(m[0].score>.9);
 strcpy(i.product_text,"clinic plus shampoo");n=sm_match_product_hybrid(&i,p,3,m,3,NULL,NULL);assert(strcmp(m[0].sku,"C")==0);
 return 0;
}
