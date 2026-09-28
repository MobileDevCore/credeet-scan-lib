#include "scanmatch.h"
#include <assert.h>
#include <string.h>
static void t(const char*s,double q,const char*u,const char*p){SM_ListItem x;assert(sm_parse_line(s,&x)==0);assert(x.quantity==q);assert(strcmp(x.unit,u)==0);assert(strcmp(x.product_text,p)==0);}
int main(void){t("2 kg rice",2,"kg","rice");t("Rice 5 kg",5,"kg","Rice");t("rice 2kg",2,"kg","rice");t("rice - 2 kg",2,"kg","rice");t("Soap x 4",4,"piece","Soap");t("Shampoo 2 bottles",2,"bottle","Shampoo");t("500 g badam",500,"g","badam");t("2 किलो rice",2,"kg","rice");t("2 કિલો ચોખા",2,"kg","ચોખા");return 0;}
