#include "scanmatch_text.h"
#include "scanmatch.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static size_t cp_len(const unsigned char *s){
    if (!s || !*s) return 0;
    if (*s < 0x80) return 1;
    if ((*s & 0xE0) == 0xC0) return 2;
    if ((*s & 0xF0) == 0xE0) return 3;
    if ((*s & 0xF8) == 0xF0) return 4;
    return 1;
}
static unsigned cp_decode(const unsigned char *s,size_t n){
    if (!s || !n) return 0xFFFD;
    if (n==1) return s[0];
    if (n==2) return ((unsigned)(s[0]&0x1F)<<6)|(s[1]&0x3F);
    if (n==3) return ((unsigned)(s[0]&0x0F)<<12)|((unsigned)(s[1]&0x3F)<<6)|(s[2]&0x3F);
    return ((unsigned)(s[0]&0x07)<<18)|((unsigned)(s[1]&0x3F)<<12)|((unsigned)(s[2]&0x3F)<<6)|(s[3]&0x3F);
}
static int is_space_cp(unsigned cp){return cp==' '||cp=='\t'||cp=='\r'||cp=='\n'||cp==0x00A0;}
static int is_punct_cp(unsigned cp){return cp<128 && (ispunct((unsigned char)cp));}
static int guj_digit(unsigned cp){return cp>=0x0AE6&&cp<=0x0AEF?(int)(cp-0x0AE6):-1;}
static int dev_digit(unsigned cp){return cp>=0x0966&&cp<=0x096F?(int)(cp-0x0966):-1;}
static void emit_ascii(char *out,size_t cap,size_t *j,char c){if(*j+1<cap)out[(*j)++]=c;}
size_t sm_normalize_digits(const char *input,char *output,size_t capacity){
    if(!output||capacity==0)return 0;size_t j=0;
    if(!input){output[0]=0;return 0;}
    for(size_t i=0;input[i];){size_t n=cp_len((const unsigned char*)input+i);unsigned cp=cp_decode((const unsigned char*)input+i,n);int d=guj_digit(cp);if(d<0)d=dev_digit(cp);if(d>=0)emit_ascii(output,capacity,&j,(char)('0'+d));else{if(j+n<capacity){memcpy(output+j,input+i,n);j+=n;}else break;}i+=n;}
    output[j]=0;return j;
}
size_t sm_normalize_text(const char *input,char *output,size_t capacity){
    if(!output||capacity==0)return 0;output[0]=0;if(!input)return 0;size_t j=0;int space=1;
    char digitbuf[SM_MAX_TEXT*2];sm_normalize_digits(input,digitbuf,sizeof(digitbuf));
    for(size_t i=0;digitbuf[i];){size_t n=cp_len((const unsigned char*)digitbuf+i);unsigned cp=cp_decode((const unsigned char*)digitbuf+i,n);
        if(cp<128){
            if(isalnum((unsigned char)cp)){emit_ascii(output,capacity,&j,(char)tolower((unsigned char)cp));space=0;}
            else if(!space){emit_ascii(output,capacity,&j,' ');space=1;}
        }else if(is_space_cp(cp)||is_punct_cp(cp)){
            if(!space){emit_ascii(output,capacity,&j,' ');space=1;}
        }else{
            if(j+n<capacity){memcpy(output+j,digitbuf+i,n);j+=n;space=0;}else break;
        }
        i+=n;
    }
    if(j&&output[j-1]==' ')j--;output[j]=0;return j;
}
int sm_detect_language(const char *text,char *output,size_t capacity){
    if(!output||capacity==0)return -1;size_t guj=0,dev=0,latin=0;
    if(text)for(size_t i=0;text[i];){size_t n=cp_len((const unsigned char*)text+i);unsigned cp=cp_decode((const unsigned char*)text+i,n);if(cp>=0x0A80&&cp<=0x0AFF)guj++;else if(cp>=0x0900&&cp<=0x097F)dev++;else if(cp<128&&isalpha((unsigned char)cp))latin++;i+=n;}
    const char *r=guj>=dev&&guj>0?"guj":dev>0?"hin":latin>0?"eng":"unknown";snprintf(output,capacity,"%s",r);return 0;
}
