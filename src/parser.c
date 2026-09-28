#include "scanmatch.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

static void trim(char*s){size_t n=strlen(s);while(n&&isspace((unsigned char)s[n-1]))s[--n]=0;size_t i=0;while(s[i]&&isspace((unsigned char)s[i]))i++;if(i)memmove(s,s+i,strlen(s+i)+1);}
static int boundary(const char*p,size_t n){unsigned char c=(unsigned char)p[n];return c==0||isspace(c)||c==','||c=='-'||c==')'||c=='('||c==':';}
static int ascii_unit(const char*p,char*out,size_t cap){
 static const char*u[][2]={{"kilograms","kg"},{"kilogram","kg"},{"kgs","kg"},{"kg","kg"},{"grams","g"},{"gram","g"},{"gm","g"},{"g","g"},{"litres","liter"},{"litre","liter"},{"liters","liter"},{"liter","liter"},{"l","liter"},{"millilitres","ml"},{"millilitre","ml"},{"milliliters","ml"},{"milliliter","ml"},{"ml","ml"},{"pieces","piece"},{"piece","piece"},{"pcs","piece"},{"pc","piece"},{"packet","packet"},{"packets","packet"},{"pack","packet"},{"box","box"},{"boxes","box"},{"bottle","bottle"},{"bottles","bottle"},{"dozen","dozen"}};
 for(size_t i=0;i<sizeof(u)/sizeof(u[0]);i++){size_t n=strlen(u[i][0]);if(strncasecmp(p,u[i][0],n)==0&&boundary(p,n)){snprintf(out,cap,"%s",u[i][1]);return(int)n;}}return 0;
}
static int guj_or_hi_unit(const char*p,char*out,size_t cap){
 static const char*u[][2]={
  {"કિલોગ્રામ","kg"},{"કલોગ્રામ","kg"},{"કિલોગરામ","kg"},{"કિગ્રા","kg"},{"કિ.ગ્રા.","kg"},
  {"મિલીલીટર","ml"},{"મિલીલિટર","ml"},{"મીલીલીટર","ml"},{"મિલિ","ml"},{"મીલી","ml"},
  {"કિલો","kg"},{"કીલો","kg"},{"કલો","kg"},{"કલા","kg"},{"કિલા","kg"},{"કલો.","kg"},
  {"ગ્રામ","g"},{"ગરામ","g"},{"ગ્રા","g"},{"ગ્રા.","g"},
  {"લિટર","liter"},{"લીટર","liter"},{"લટિર","liter"},{"લટર","liter"},{"લીટિર","liter"},{"લીટર.","liter"},{"લિટર.","liter"},
  {"નંગો","piece"},{"નંગ","piece"},{"નગ","piece"},
  {"પીસો","piece"},{"પીસ","piece"},
  {"ડઝન","dozen"},{"ડજન","dozen"},
  {"પેકેટ","packet"},{"પેક","packet"},
  {"બોટલ","bottle"},{"બોતલ","bottle"},
  {"બોક્સ","box"},{"બોકસ","box"},
  {"किलोग्राम","kg"},{"किलो","kg"},{"कलि","kg"},{"किग्रा","kg"},
  {"मिलीलीटर","ml"},{"मिली","ml"},
  {"लीटर","liter"},{"लिटर","liter"},
  {"ग्राम","g"},{"ग्रा","g"},
  {"नंग","piece"},{"पीस","piece"},
  {"दर्जन","dozen"},{"पैकेट","packet"},{"पैक","packet"},{"बोतल","bottle"},{"डिब्बा","box"}
 };
 for(size_t i=0;i<sizeof(u)/sizeof(u[0]);i++){size_t n=strlen(u[i][0]);if(strncmp(p,u[i][0],n)==0&&boundary(p,n)){snprintf(out,cap,"%s",u[i][1]);return(int)n;}}return 0;
}
static int unit_at(const char*p,char*out,size_t cap){int n=ascii_unit(p,out,cap);if(n)return n;return guj_or_hi_unit(p,out,cap);}
static int utf8_digit(const unsigned char*p,size_t*n){if(p[0]>=0x30&&p[0]<=0x39){*n=1;return p[0]-0x30;}if(p[0]>=0xE0&&p[0]<=0xEF){if(!strncmp((const char*)p,"૦",3)){*n=3;return 0;}if(!strncmp((const char*)p,"૧",3)){*n=3;return 1;}if(!strncmp((const char*)p,"૨",3)){*n=3;return 2;}if(!strncmp((const char*)p,"૩",3)){*n=3;return 3;}if(!strncmp((const char*)p,"૪",3)){*n=3;return 4;}if(!strncmp((const char*)p,"૫",3)){*n=3;return 5;}if(!strncmp((const char*)p,"૬",3)){*n=3;return 6;}if(!strncmp((const char*)p,"૭",3)){*n=3;return 7;}if(!strncmp((const char*)p,"૮",3)){*n=3;return 8;}if(!strncmp((const char*)p,"૯",3)){*n=3;return 9;}if(!strncmp((const char*)p,"०",3)){*n=3;return 0;}if(!strncmp((const char*)p,"१",3)){*n=3;return 1;}if(!strncmp((const char*)p,"२",3)){*n=3;return 2;}if(!strncmp((const char*)p,"३",3)){*n=3;return 3;}if(!strncmp((const char*)p,"४",3)){*n=3;return 4;}if(!strncmp((const char*)p,"५",3)){*n=3;return 5;}if(!strncmp((const char*)p,"६",3)){*n=3;return 6;}if(!strncmp((const char*)p,"७",3)){*n=3;return 7;}if(!strncmp((const char*)p,"८",3)){*n=3;return 8;}if(!strncmp((const char*)p,"९",3)){*n=3;return 9;}}*n=0;return-1;}
static int parse_num(const char*p,double*q,size_t*used){if((unsigned char)p[0]==0xE0 && !strncmp(p,"પ",3)){*q=5;*used=3;return 1;}char tmp[64];size_t i=0,j=0;int digits=0,dot=0;while(p[i]&&j<sizeof(tmp)-1){size_t n=0;int d=utf8_digit((const unsigned char*)p+i,&n);if(d>=0){tmp[j++]=(char)('0'+d);i+=n;digits=1;continue;}if((p[i]=='.'||p[i]==',')&&!dot&&digits){tmp[j++]='.';i++;dot=1;continue;}break;}if(!digits)return 0;tmp[j]=0;char*e=NULL;double v=strtod(tmp,&e);if(!e||*e||v<=0)return 0;*q=v;*used=i;return 1;}
static void set_lang(const char*s,SM_ListItem*out){char lang[16];sm_detect_language(s,lang,sizeof(lang));snprintf(out->language,sizeof(out->language),"%s",lang);}
int sm_parse_line(const char*line,SM_ListItem*out){
 if(!line||!out)return-1;memset(out,0,sizeof(*out));out->quantity=1.0;set_lang(line,out);char buf[SM_MAX_TEXT];snprintf(buf,sizeof(buf),"%s",line);trim(buf);if(!buf[0])return-2;
 char norm[SM_MAX_TEXT];sm_normalize_digits(buf,norm,sizeof(norm));snprintf(buf,sizeof(buf),"%s",norm);char*p=buf;double q=0;size_t used=0;
 if(parse_num(p,&q,&used)){out->quantity=q;p+=used;while(*p&&isspace((unsigned char)*p))p++;char u[32]={0};int ul=unit_at(p,u,sizeof(u));if(ul){snprintf(out->unit,sizeof(out->unit),"%s",u);p+=ul;while(*p&&isspace((unsigned char)*p))p++;}}
 snprintf(out->product_text,sizeof(out->product_text),"%s",p);trim(out->product_text);size_t L=strlen(out->product_text);if(L&&out->product_text[L-1]=='-'){out->product_text[--L]=0;trim(out->product_text);}
 for(size_t i=L;i>0;i--){if(out->product_text[i-1]=='x'||out->product_text[i-1]=='X'){char*z=out->product_text+i;while(*z&&isspace((unsigned char)*z))z++;double qx=0;size_t ux=0;if(*z&&parse_num(z,&qx,&ux)&&z[ux]==0){out->quantity=qx;snprintf(out->unit,sizeof(out->unit),"piece");out->product_text[i-1]=0;trim(out->product_text);return 0;}break;}}
 L=strlen(out->product_text);size_t ts=L;while(ts&&!isspace((unsigned char)out->product_text[ts-1]))ts--;char*tok=out->product_text+ts;double qa=0;size_t ua=0;if(ts<L&&parse_num(tok,&qa,&ua)&&qa>0){char u[32]={0};int ul=unit_at(tok+ua,u,sizeof(u));if(ul&&(tok+ua)[ul]==0){out->quantity=qa;snprintf(out->unit,sizeof(out->unit),"%s",u);*tok=0;trim(out->product_text);return 0;}}
 L=strlen(out->product_text);size_t uend=L;while(uend&&isspace((unsigned char)out->product_text[uend-1]))uend--;size_t us=uend;while(us&&!isspace((unsigned char)out->product_text[us-1]))us--;char u[32]={0};int ul=unit_at(out->product_text+us,u,sizeof(u));if(ul&&(size_t)ul==uend-us){size_t ns=us;while(ns&&isspace((unsigned char)out->product_text[ns-1]))ns--;size_t ne=ns;while(ne&&isspace((unsigned char)out->product_text[ne-1]))ne--;size_t nstart=ne;while(nstart&&!isspace((unsigned char)out->product_text[nstart-1]))nstart--;double q2=0;size_t uq=0;if(nstart<ne&&parse_num(out->product_text+nstart,&q2,&uq)&&nstart+uq==ne){out->quantity=q2;snprintf(out->unit,sizeof(out->unit),"%s",u);out->product_text[nstart]=0;trim(out->product_text);size_t z=strlen(out->product_text);while(z&&(out->product_text[z-1]=='-'||out->product_text[z-1]==':'))out->product_text[--z]=0;trim(out->product_text);}}
 return 0;
}
