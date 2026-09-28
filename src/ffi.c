#include "scanmatch.h"
#include "scanmatch_ffi.h"
#include "scanmatch_ocr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

struct SM_Context { SM_Product products[SM_MAX_PRODUCTS]; size_t product_count; char error[512]; };
static void seterr(SM_Context*c,const char*f,...){if(!c)return;va_list a;va_start(a,f);vsnprintf(c->error,sizeof(c->error),f,a);va_end(a);}
SM_Context*sm_create_context(void){return calloc(1,sizeof(SM_Context));}
void sm_destroy_context(SM_Context*c){free(c);}
int sm_load_catalog_csv_ffi(SM_Context*c,const char*p){if(!c||!p)return -1;int rc=sm_load_catalog_csv(p,c->products,SM_MAX_PRODUCTS,&c->product_count);if(rc)seterr(c,"CATALOG_LOAD_FAILED:%d",rc);else if(!c->product_count)seterr(c,"CATALOG_EMPTY");return rc;}
static int app(char**b,size_t*cap,size_t*len,const char*f,...){for(;;){va_list a;va_start(a,f);int n=vsnprintf(*b+*len,*cap-*len,f,a);va_end(a);if(n<0)return -1;if(*len+(size_t)n<*cap){*len+=(size_t)n;return 0;}size_t nc=*cap*2;if(nc<*len+(size_t)n+1)nc=*len+(size_t)n+1;if(nc>4*1024*1024)return -2;char*nb=realloc(*b,nc);if(!nb)return -3;*b=nb;*cap=nc;}}
static int jsons(char**b,size_t*cap,size_t*len,const char*s){if(app(b,cap,len,"\""))return -1;for(const unsigned char*p=(const unsigned char*)(s?s:"");*p;p++){if(*p=='"'||*p=='\\')app(b,cap,len,"\\%c",*p);else if(*p=='\n')app(b,cap,len,"\\n");else if(*p=='\r')app(b,cap,len,"\\r");else if(*p=='\t')app(b,cap,len,"\\t");else if(*p<32)app(b,cap,len," ");else app(b,cap,len,"%c",*p);}return app(b,cap,len,"\"");}

int sm_scan_image_json(SM_Context*c,const char*path,const char*languages,char**out){
 if(!c||!path||!out)return -1;*out=NULL;if(c->product_count==0){seterr(c,"CATALOG_EMPTY");return -2;}
 SM_OCR_Config cfg={languages&&*languages?languages:"guj+eng+hin",6,1,2,NULL};SM_OCR_Result o;int rc=sm_ocr_image_config(path,&cfg,&o);
 if(rc){seterr(c,rc==-2?"INVALID_IMAGE":"OCR_FAILED:%d",rc);return rc;}
 if(o.width<=0||o.height<=0||((long long)o.width*o.height)>25000000LL){sm_ocr_result_free(&o);seterr(c,"IMAGE_DIMENSIONS_UNSUPPORTED");return -4;}
 char detected[16];sm_detect_language(o.text,detected,sizeof(detected));
 size_t cap=8192,len=0;char*j=malloc(cap);if(!j){sm_ocr_result_free(&o);seterr(c,"OUT_OF_MEMORY");return -3;}
 app(&j,&cap,&len,"{\"success\":true,\"language\":");jsons(&j,&cap,&len,detected);app(&j,&cap,&len,",\"raw_text\":");jsons(&j,&cap,&len,o.text);app(&j,&cap,&len,",\"ocr_confidence\":%.3f,\"rotation_degrees\":%d,\"image_quality\":{\"score\":%.3f,\"usable\":%s,\"reason\":",o.mean_confidence,o.rotation_degrees,o.quality_score,o.quality_score>=.50?"true":"false");jsons(&j,&cap,&len,o.quality_reason);app(&j,&cap,&len,"},\"ocr_time_ms\":%lld,\"orientation_detection_ms\":%lld,\"preprocessing_ms\":%lld,\"ocr_ms\":%lld,\"fallback_ocr_ms\":%lld,\"total_ocr_pipeline_ms\":%lld,\"ocr_passes\":%d,\"items\":[",o.ocr_time_ms,o.orientation_detection_ms,o.preprocessing_ms,o.ocr_ms,o.fallback_ocr_ms,o.total_ocr_pipeline_ms,o.ocr_passes);
 char*save=NULL;int first=1;clock_t matching_total=0;
 for(char*line=strtok_r(o.text,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
   char normalized[SM_MAX_TEXT];sm_normalize_text(line,normalized,sizeof(normalized));SM_ListItem item;if(sm_parse_line(line,&item)!=0||!item.product_text[0])continue;item.ocr_confidence=o.mean_confidence;
   SM_ProductMatch m[SM_MAX_CANDIDATES];clock_t st=clock();int n=sm_match_product_hybrid(&item,c->products,c->product_count,m,SM_MAX_CANDIDATES,NULL,NULL);matching_total+=clock()-st;
   if(!first)app(&j,&cap,&len,",");first=0;app(&j,&cap,&len,"{\"raw_text\":");jsons(&j,&cap,&len,line);app(&j,&cap,&len,",\"normalized_text\":");jsons(&j,&cap,&len,normalized);app(&j,&cap,&len,",\"product_text\":");jsons(&j,&cap,&len,item.product_text);app(&j,&cap,&len,",\"quantity\":%.3f,\"unit\":",item.quantity);jsons(&j,&cap,&len,item.unit);
   if(n<=0||m[0].score<.55){app(&j,&cap,&len,",\"product_id\":null,\"name\":null,\"confidence\":0,\"status\":\"unidentified\",\"needs_confirmation\":true,\"candidates\":[]}");continue;}
   int confirm=(m[0].score<.95)||(n>1&&(m[0].score-m[1].score)<.12);const char*status=confirm?"needs_confirmation":"confirmed";
   app(&j,&cap,&len,",\"product_id\":");jsons(&j,&cap,&len,m[0].sku);app(&j,&cap,&len,",\"name\":");jsons(&j,&cap,&len,m[0].canonical_name);app(&j,&cap,&len,",\"confidence\":%.4f,\"status\":\"%s\",\"needs_confirmation\":%s,\"candidates\":[",m[0].score,status,confirm?"true":"false");
   for(int k=0;k<n;k++){if(k)app(&j,&cap,&len,",");app(&j,&cap,&len,"{\"product_id\":");jsons(&j,&cap,&len,m[k].sku);app(&j,&cap,&len,",\"name\":");jsons(&j,&cap,&len,m[k].canonical_name);app(&j,&cap,&len,",\"score\":%.4f,\"confidence\":\"%s\"}",m[k].score,m[k].confidence_band);}app(&j,&cap,&len,"]}");
 }
 long long match_ms=(long long)((double)matching_total*1000.0/CLOCKS_PER_SEC);long long total_ms=o.ocr_time_ms+match_ms;app(&j,&cap,&len,"] ,\"matching_time_ms\":%lld,\"total_time_ms\":%lld}",match_ms,total_ms);
 sm_ocr_result_free(&o);*out=j;return 0;
}
const char*sm_last_error(SM_Context*c){return c?c->error:"INVALID_CONTEXT";}void sm_free_string(char*s){free(s);}
