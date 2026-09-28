#include "scanmatch.h"
#include <assert.h>
#include <string.h>
static void t(const char*s,double q,const char*u,const char*p){SM_ListItem x;assert(sm_parse_line(s,&x)==0);assert(x.quantity==q);assert(strcmp(x.unit,u)==0);assert(strcmp(x.product_text,p)==0);}
int main(void){
 t("ચોખા ૫ કિલો",5,"kg","ચોખા");
 t("ચોખા પ કિલો",5,"kg","ચોખા");
 t("ચોખા ૨૫ કિલોગ્રામ",25,"kg","ચોખા");
 t("ખાંડ ૫૦૦ ગ્રામ",500,"g","ખાંડ");
 t("દૂધ ૧ લિટર",1,"liter","દૂધ");
 t("સાબુ ૪ નંગ",4,"piece","સાબુ");
 t("ચા ૫૦૦ g",500,"g","ચા");
 t("ચોખા 1.5 કિલો",1.5,"kg","ચોખા");
 t("ચોખા 5 કિલો",5,"kg","ચોખા");
 t("ચોખા 5 કલો",5,"kg","ચોખા");
 t("ચોખા 5 કલા",5,"kg","ચોખા");
 t("દૂધ 2 લિટર",2,"liter","દૂધ");
 t("દૂધ 2 લટિર",2,"liter","દૂધ");
 t("દૂધ 2 લટર",2,"liter","દૂધ");
 t("દૂધ 2 લીટર",2,"liter","દૂધ");
 char n[128];sm_normalize_digits("૨૫ ૫૦૦",n,sizeof(n));assert(strcmp(n,"25 500")==0);
 sm_normalize_text("  ચોખા,  ૫  કિલો! ",n,sizeof(n));assert(strcmp(n,"ચોખા 5 કિલો")==0);
 return 0;
}
