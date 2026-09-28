#include "scanmatch.h"
double sm_embedding_blend(double text_score,double embedding_score){
    return embedding_score < 0.0 ? text_score : 0.5*text_score + 0.5*embedding_score;
}
