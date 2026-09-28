#include "scanmatch_ocr.h"
#include "scanmatch_text.h"
#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
extern PIX*sm_preprocess_pix(PIX*,int,int);
static long long nowms(void){return(long long)((double)clock()*1000.0/CLOCKS_PER_SEC);}
static int run_pass(PIX*pix,const char*langs,int psm,char**text,double*conf){TessBaseAPI*a=TessBaseAPICreate();if(!a)return-1;if(TessBaseAPIInit3(a,NULL,langs&&*langs?langs:"guj")!=0){TessBaseAPIDelete(a);return-2;}TessBaseAPISetPageSegMode(a,(TessPageSegMode)psm);TessBaseAPISetImage2(a,pix);if(TessBaseAPIRecognize(a,NULL)!=0){TessBaseAPIEnd(a);TessBaseAPIDelete(a);return-4;}const char*t=TessBaseAPIGetUTF8Text(a);if(!t){TessBaseAPIEnd(a);TessBaseAPIDelete(a);return-5;}size_t n=strlen(t);*text=malloc(n+1);if(!*text){TessDeleteText(t);TessBaseAPIEnd(a);TessBaseAPIDelete(a);return-6;}memcpy(*text,t,n+1);TessDeleteText(t);*conf=(double)TessBaseAPIMeanTextConf(a)/100.0;TessBaseAPIEnd(a);TessBaseAPIDelete(a);return 0;}
static int looks_useful(const char*t){if(!t)return 0;int n=0;for(const unsigned char*p=(const unsigned char*)t;*p;p++)if(!isspace(*p))n++;return n>=2;}

static char* dup_tess_text(const char* s){
    if(!s) return NULL;
    size_t n = strlen(s);
    char* d = malloc(n + 1);
    if(d) memcpy(d, s, n + 1);
    return d;
}

int sm_ocr_image_config(const char*path,const SM_OCR_Config*c,SM_OCR_Result*out){
    if(!path||!out)return -1;
    memset(out,0,sizeof(*out));
    long long t0=nowms();
    PIX*src=pixRead(path);
    if(!src)return -2;
    out->width=pixGetWidth(src);
    out->height=pixGetHeight(src);

    /* Quality assessment */
    PIX*g=pixConvertTo8(src,0);
    l_float32 root=0;
    if(g){pixVarianceInRect(g,NULL,&root);}
    if(root<20)out->quality_score=.25;
    else if(root<50)out->quality_score=.55;
    else if(root<90)out->quality_score=.80;
    else out->quality_score=1.0;
    snprintf(out->quality_reason,sizeof(out->quality_reason),"%s",
             out->quality_score<.5?"low contrast; retake recommended":
             out->quality_score<.75?"moderate contrast":"usable image quality");

    /* Stage 1: Orientation detection */
    long long t_orient=nowms();
    int best_rot=0;
    if(g && pixGetWidth(g)>=150 && pixGetHeight(g)>=150){
        PIX*b=pixThresholdToBinary(g,160);
        if(b){
            l_float32 upconf=0.0f,leftconf=0.0f;
            l_int32 rot_dec=0;
            if(pixOrientCorrect(b,2.0f,2.5f,&upconf,&leftconf,&rot_dec,0)!=NULL){
                if(rot_dec==1)best_rot=90;
                else if(rot_dec==2)best_rot=180;
                else if(rot_dec==3)best_rot=270;
            }
            pixDestroy(&b);
        }
    }
    if(g)pixDestroy(&g);
    out->orientation_detection_ms=nowms()-t_orient;
    out->rotation_degrees=best_rot;

    PIX*oriented=(best_rot==0)?pixClone(src):pixRotateOrth(src,best_rot/90);
    if(!oriented)oriented=pixClone(src);

    /* Stage 2: Primary preprocessing & Gujarati-first OCR */
    long long t_prep=nowms();
    const char*requested=c&&c->languages&&*c->languages?c->languages:"eng+hin+guj";
    int psm=c&&c->psm?c->psm:6,scale=c&&c->scale>0?c->scale:2;
    PIX*prep_pix=sm_preprocess_pix(oriented,0,scale);
    if(!prep_pix)prep_pix=pixClone(oriented);
    out->preprocessing_ms=nowms()-t_prep;

    const char*tessdata=c&&c->tessdata_path&&*c->tessdata_path?c->tessdata_path:getenv("SCANMATCH_TESSDATA");
    if(!tessdata||!*tessdata)tessdata=getenv("TESSDATA_PREFIX");

    const char*primary_lang=strstr(requested,"guj")?"guj":requested;
    TessBaseAPI*api=TessBaseAPICreate();
    if(!api){pixDestroy(&prep_pix);pixDestroy(&oriented);pixDestroy(&src);return -1;}

    if(TessBaseAPIInit3(api,tessdata,primary_lang)!=0){
        /* If specific primary language failed, try default/eng */
        if(TessBaseAPIInit3(api,tessdata,NULL)!=0){
            TessBaseAPIDelete(api);pixDestroy(&prep_pix);pixDestroy(&oriented);pixDestroy(&src);return -2;
        }
    }

    long long t_ocr=nowms();
    TessBaseAPISetPageSegMode(api,(TessPageSegMode)psm);
    TessBaseAPISetImage2(api,prep_pix);
    char*best_text=NULL;
    double best_conf=-1.0;
    int passes=0;

    if(TessBaseAPIRecognize(api,NULL)==0){
        const char*t=TessBaseAPIGetUTF8Text(api);
        if(t){
            best_text=dup_tess_text(t);
            best_conf=(double)TessBaseAPIMeanTextConf(api)/100.0;
            TessDeleteText(t);
        }
    }
    passes++;
    out->ocr_ms=nowms()-t_ocr;

    /* Stage 3: Result validation & selective fallback */
    long long t_fb=nowms();
    char detected[16]={0};
    if(best_text)sm_detect_language(best_text,detected,sizeof(detected));

    int primary_good=(best_text && looks_useful(best_text) && best_conf>=0.70);
    if(primary_good && strcmp(primary_lang,"guj")==0){
        if(strcmp(detected,"guj")!=0 && strcmp(detected,"mixed")!=0 && strcmp(requested,"guj")!=0){
            primary_good=0; /* Multilingual fallback needed */
        }
    }

    if(!primary_good){
        /* Fallback A: Multilingual pass if requested has more languages than primary */
        if(strcmp(primary_lang,requested)!=0){
            if(TessBaseAPIInit3(api,tessdata,requested)==0){
                TessBaseAPISetPageSegMode(api,(TessPageSegMode)psm);
                TessBaseAPISetImage2(api,prep_pix);
                if(TessBaseAPIRecognize(api,NULL)==0){
                    const char*t=TessBaseAPIGetUTF8Text(api);
                    if(t){
                        double fb_conf=(double)TessBaseAPIMeanTextConf(api)/100.0;
                        if(looks_useful(t)&&(fb_conf>best_conf || !best_text || !looks_useful(best_text))){
                            free(best_text);
                            best_text=dup_tess_text(t);
                            best_conf=fb_conf;
                        }
                        TessDeleteText(t);
                    }
                }
                passes++;
            }
        }

        /* Fallback B: Alternate preprocessing mode if still low confidence or empty */
        if((!best_text || !looks_useful(best_text) || best_conf<0.60) && c && c->preprocess){
            PIX*prep_fb=sm_preprocess_pix(oriented,1,scale);
            if(prep_fb){
                TessBaseAPISetImage2(api,prep_fb);
                if(TessBaseAPIRecognize(api,NULL)==0){
                    const char*t=TessBaseAPIGetUTF8Text(api);
                    if(t){
                        double fb_conf=(double)TessBaseAPIMeanTextConf(api)/100.0;
                        if(looks_useful(t)&&(fb_conf>best_conf || !best_text || !looks_useful(best_text))){
                            free(best_text);
                            best_text=dup_tess_text(t);
                            best_conf=fb_conf;
                        }
                        TessDeleteText(t);
                    }
                }
                passes++;
                pixDestroy(&prep_fb);
            }
        }

        /* Fallback C: Orientation recovery only if STILL no readable text at all */
        if((!best_text || !looks_useful(best_text) || best_conf<0.35) && best_rot==0){
            for(int r_try=1;r_try<4;r_try++){
                PIX*r=pixRotateOrth(src,r_try);
                if(!r)continue;
                PIX*pr=sm_preprocess_pix(r,0,scale);
                if(pr){
                    TessBaseAPISetImage2(api,pr);
                    if(TessBaseAPIRecognize(api,NULL)==0){
                        const char*t=TessBaseAPIGetUTF8Text(api);
                        if(t){
                            double r_conf=(double)TessBaseAPIMeanTextConf(api)/100.0;
                            if(looks_useful(t)&&r_conf>best_conf){
                                free(best_text);
                                best_text=dup_tess_text(t);
                                best_conf=r_conf;
                                out->rotation_degrees=r_try*90;
                            }
                            TessDeleteText(t);
                        }
                    }
                    passes++;
                    pixDestroy(&pr);
                }
                pixDestroy(&r);
                if(best_conf>=0.70)break;
            }
        }
    }
    out->fallback_ocr_ms=nowms()-t_fb;
    out->ocr_passes=passes;
    out->ocr_time_ms=nowms()-t0;
    out->total_ocr_pipeline_ms=out->ocr_time_ms;

    pixDestroy(&prep_pix);
    pixDestroy(&oriented);
    pixDestroy(&src);
    TessBaseAPIEnd(api);
    TessBaseAPIDelete(api);

    if(!best_text)return -3;
    out->text=best_text;
    out->mean_confidence=best_conf>=0.0?best_conf:0.0;
    return 0;
}
void sm_ocr_result_free(SM_OCR_Result*r){if(!r)return;free(r->text);memset(r,0,sizeof(*r));}
