#include "scanmatch_ocr.h"
#include <leptonica/allheaders.h>

static PIX *crop_border(PIX *src){
    l_int32 w=pixGetWidth(src),h=pixGetHeight(src); if(w<100||h<100)return pixClone(src);
    l_int32 bx=w/100,by=h/100; BOX *b=boxCreate(bx,by,w-2*bx,h-2*by); PIX *r=pixClipRectangle(src,b,NULL); boxDestroy(&b); return r?r:pixClone(src);
}
static PIX *prep(PIX*src,int mode,int scale){
    if(!src)return NULL;
    PIX *c=crop_border(src); if(!c)return NULL;
    PIX *g=pixConvertTo8(c,0);pixDestroy(&c);if(!g)return NULL;
    PIX *w=g;
    if(scale>1){PIX*s=pixScale(g,(l_float32)scale,(l_float32)scale);if(s){pixDestroy(&w);w=s;}}
    if(mode==1){PIX*t=NULL,*d=NULL;if(pixOtsuAdaptiveThreshold(w,200,200,0,0,0.10,&t,&d)==0&&t){pixDestroy(&w);w=t;}if(d)pixDestroy(&d);}
    else if(mode==2){PIX*t=pixThresholdToBinary(w,180);if(t){pixDestroy(&w);w=t;}}
    else if(mode==3){PIX*t=pixThresholdToBinary(w,128);if(t){pixDestroy(&w);w=t;}}
    if(pixGetWidth(w)>=150 && pixGetHeight(w)>=150){PIX*d=pixDeskew(w,0);if(d){pixDestroy(&w);w=d;}}
    return w;
}
PIX*sm_preprocess_pix(PIX*src,int mode,int scale){return prep(src,mode,scale);}
