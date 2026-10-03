#include "scanmatch.h"
#include "scanmatch_text.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

static int is_word_boundary(const char *p, size_t n) {
    unsigned char c = (unsigned char)p[n];
    return c == 0 || isspace(c) || c == ',' || c == '-' || c == ')' || c == '(' || c == ':';
}

static int match_ascii_unit(const char *p, char *out, size_t cap) {
    static const char *units[][2] = {
        {"kilograms", "kg"}, {"kilogram", "kg"}, {"kgs", "kg"}, {"kg", "kg"},
        {"grams", "g"}, {"gram", "g"}, {"gms", "g"}, {"gm", "g"}, {"g", "g"},
        {"litres", "liter"}, {"litre", "liter"}, {"liters", "liter"}, {"liter", "liter"}, {"l", "liter"},
        {"millilitres", "ml"}, {"millilitre", "ml"}, {"milliliters", "ml"}, {"milliliter", "ml"}, {"ml", "ml"},
        {"pieces", "piece"}, {"piece", "piece"}, {"pcs", "piece"}, {"pc", "piece"},
        {"packets", "packet"}, {"packet", "packet"}, {"packs", "packet"}, {"pack", "packet"},
        {"pukat", "packet"}, {"puket", "packet"}, {"paket", "packet"},
        {"boxes", "box"}, {"box", "box"},
        {"bottles", "bottle"}, {"bottle", "bottle"}, {"bot", "bottle"},
        {"dozen", "dozen"}, {"doz", "dozen"},
        {"theli", "bag"}, {"thelee", "bag"}, {"bags", "bag"}, {"bag", "bag"}, {"bori", "bag"}
    };
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
        size_t n = strlen(units[i][0]);
        if (strncasecmp(p, units[i][0], n) == 0 && is_word_boundary(p, n)) {
            snprintf(out, cap, "%s", units[i][1]);
            return (int)n;
        }
    }
    return 0;
}

static int match_indic_unit(const char *p, char *out, size_t cap) {
    static const char *units[][2] = {
        /* Gujarati units */
        {"કિલોગ્રામ", "kg"}, {"કલોગ્રામ", "kg"}, {"કિલોગરામ", "kg"}, {"કિગ્રા", "kg"}, {"કિ.ગ્રા.", "kg"},
        {"મિલીલીટર", "ml"}, {"મિલીલિટર", "ml"}, {"મીલીલીટર", "ml"}, {"મિલિ", "ml"}, {"મીલી", "ml"},
        {"કિલો", "kg"}, {"કીલો", "kg"}, {"કલો", "kg"}, {"કલા", "kg"}, {"કિલા", "kg"}, {"કલો.", "kg"},
        {"ગ્રામ", "g"}, {"ગરામ", "g"}, {"ગ્રા", "g"}, {"ગ્રા.", "g"},
        {"લિટર", "liter"}, {"લીટર", "liter"}, {"લટિર", "liter"}, {"લટર", "liter"}, {"લીટિર", "liter"},
        {"લીટર.", "liter"}, {"લિટર.", "liter"},
        {"નંગો", "piece"}, {"નંગ", "piece"}, {"નગ", "piece"},
        {"પીસો", "piece"}, {"પીસ", "piece"},
        {"ડઝન", "dozen"}, {"ડજન", "dozen"},
        {"પેકેટ", "packet"}, {"પેક", "packet"},
        {"થેલીઓ", "bag"}, {"થેલી", "bag"}, {"બોરી", "bag"},
        {"બોટલ", "bottle"}, {"બોતલ", "bottle"},
        {"બોક્સ", "box"}, {"બોકસ", "box"},
        /* Hindi units */
        {"किलोग्राम", "kg"}, {"किलो", "kg"}, {"कलि", "kg"}, {"किग्रा", "kg"},
        {"मिलीलीटर", "ml"}, {"मिली", "ml"},
        {"लीटर", "liter"}, {"लिटर", "liter"},
        {"ग्राम", "g"}, {"ग्रा", "g"},
        {"नंग", "piece"}, {"पीस", "piece"},
        {"दर्जन", "dozen"}, {"पैकेट", "packet"}, {"पैक", "packet"},
        {"थैली", "bag"}, {"बोतल", "bottle"}, {"डिब्बा", "box"}
    };
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
        size_t n = strlen(units[i][0]);
        if (strncmp(p, units[i][0], n) == 0 && is_word_boundary(p, n)) {
            snprintf(out, cap, "%s", units[i][1]);
            return (int)n;
        }
    }
    return 0;
}

static int unit_at(const char *p, char *out, size_t cap) {
    int n = match_ascii_unit(p, out, cap);
    if (n > 0) return n;
    return match_indic_unit(p, out, cap);
}

static int parse_num(const char *p, double *q, size_t *used) {
    /* OCR recovery: In Gujarati OCR, '૫' (U+0AEB, digit 5) is frequently misread as 'પ' (U+0AAA, Pa). */
    if ((unsigned char)p[0] == 0xE0 && !strncmp(p, "પ", 3)) {
        *q = 5.0;
        *used = 3;
        return 1;
    }

    char tmp[64];
    size_t i = 0, j = 0;
    int digits = 0, dot = 0;

    while (p[i] && j < sizeof(tmp) - 1) {
        if (p[i] >= '0' && p[i] <= '9') {
            tmp[j++] = p[i++];
            digits = 1;
            continue;
        }
        if ((p[i] == '.' || p[i] == ',') && !dot && digits) {
            tmp[j++] = '.';
            i++;
            dot = 1;
            continue;
        }
        break;
    }

    if (!digits) return 0;
    tmp[j] = '\0';
    char *e = NULL;
    double v = strtod(tmp, &e);
    if (!e || *e || v <= 0.0) return 0;
    *q = v;
    *used = i;
    return 1;
}

static void set_lang(const char *s, SM_ListItem *out) {
    char lang[16];
    sm_detect_language(s, lang, sizeof(lang));
    snprintf(out->language, sizeof(out->language), "%s", lang);
}

/* Strips trailing colloquial modifiers such as 'વાળુ' / 'વાળું' / 'વાળા' ('the ... one') */
static void strip_colloquial_suffix(char *s) {
    sm_trim(s);
    size_t len = strlen(s);
    static const char *suffixes[] = {
        "વાળું", "વાળુ", "વાળા", "વાળો", "વાલી", "walu", "wala", "wali"
    };
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        size_t slen = strlen(suffixes[i]);
        if (len >= slen) {
            if (strcmp(s + len - slen, suffixes[i]) == 0) {
                s[len - slen] = '\0';
                sm_trim(s);
                return;
            }
        }
    }
}

int sm_parse_line(const char *line, SM_ListItem *out) {
    if (!line || !out) return -1;
    memset(out, 0, sizeof(*out));
    out->has_quantity = 0;
    out->quantity = 0.0;
    out->has_unit = 0;
    out->unit[0] = '\0';

    const char *src = line;
    if ((unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) {
        src += 3;
    }

    char buf[SM_MAX_TEXT];
    snprintf(buf, sizeof(buf), "%s", src);
    sm_trim(buf);
    if (!buf[0]) return -2;

    set_lang(buf, out);

    /* Normalize Gujarati (૦-૯) and Devanagari (०-९) digits to standard ASCII ('0'-'9') */
    char norm[SM_MAX_TEXT];
    sm_normalize_digits(buf, norm, sizeof(norm));
    snprintf(buf, sizeof(buf), "%s", norm);

    /* Check and strip trailing colloquial suffix modifiers like '1 kg વાળુ' */
    strip_colloquial_suffix(buf);

    char *p = buf;
    double q = 0;
    size_t used = 0;

    /* Pattern 1: Leading quantity (e.g. "2 kg rice", "૫ કિલો ચોખા", "500 g badam") */
    if (parse_num(p, &q, &used)) {
        out->quantity = q;
        out->has_quantity = 1;
        p += used;
        while (*p && isspace((unsigned char)*p)) p++;

        char u[32] = {0};
        int ul = unit_at(p, u, sizeof(u));
        if (ul > 0) {
            snprintf(out->unit, sizeof(out->unit), "%s", u);
            out->has_unit = 1;
            p += ul;
        }
        while (*p && (isspace((unsigned char)*p) || *p == '-' || *p == ':' || *p == ',')) p++;
    }

    snprintf(out->product_text, sizeof(out->product_text), "%s", p);
    sm_trim(out->product_text);

    size_t L = strlen(out->product_text);
    while (L > 0 && (out->product_text[L - 1] == '-' || out->product_text[L - 1] == ':')) {
        out->product_text[--L] = '\0';
        sm_trim(out->product_text);
    }

    /* Pattern 2: Trailing multiplier (e.g. "Soap x 4" -> quantity: 4, unit: "piece") */
    for (size_t i = L; i > 0; i--) {
        if (out->product_text[i - 1] == 'x' || out->product_text[i - 1] == 'X') {
            char *z = out->product_text + i;
            while (*z && isspace((unsigned char)*z)) z++;
            double qx = 0;
            size_t ux = 0;
            if (*z && parse_num(z, &qx, &ux) && z[ux] == '\0') {
                out->quantity = qx;
                out->has_quantity = 1;
                snprintf(out->unit, sizeof(out->unit), "piece");
                out->has_unit = 1;
                out->product_text[i - 1] = '\0';
                sm_trim(out->product_text);
                return 0;
            }
            break;
        }
    }

    /* Pattern 3: Trailing compound token (e.g. "rice 2kg", "soap 4pcs") */
    L = strlen(out->product_text);
    size_t ts = L;
    while (ts > 0 && !isspace((unsigned char)out->product_text[ts - 1])) ts--;
    char *tok = out->product_text + ts;
    double qa = 0;
    size_t ua = 0;
    if (ts < L && parse_num(tok, &qa, &ua) && qa > 0) {
        char u[32] = {0};
        int ul = unit_at(tok + ua, u, sizeof(u));
        if (ul > 0 && (tok + ua)[ul] == '\0') {
            out->quantity = qa;
            out->has_quantity = 1;
            snprintf(out->unit, sizeof(out->unit), "%s", u);
            out->has_unit = 1;
            *tok = '\0';
            sm_trim(out->product_text);
            return 0;
        }
    }

    /* Pattern 4: Trailing separated quantity and unit (e.g. "Rice 5 kg", "ચોખા ૫ કિલો", "Sugar - 500 g") */
    L = strlen(out->product_text);
    size_t uend = L;
    while (uend > 0 && isspace((unsigned char)out->product_text[uend - 1])) uend--;
    size_t us = uend;
    while (us > 0 && !isspace((unsigned char)out->product_text[us - 1])) us--;

    char u[32] = {0};
    int ul = unit_at(out->product_text + us, u, sizeof(u));
    if (ul > 0 && (size_t)ul == (uend - us)) {
        size_t ns = us;
        while (ns > 0 && isspace((unsigned char)out->product_text[ns - 1])) ns--;
        size_t ne = ns;
        size_t nstart = ne;
        while (nstart > 0 && !isspace((unsigned char)out->product_text[nstart - 1])) nstart--;

        double q2 = 0;
        size_t uq = 0;
        if (nstart < ne && parse_num(out->product_text + nstart, &q2, &uq) && (nstart + uq) == ne) {
            out->quantity = q2;
            out->has_quantity = 1;
            snprintf(out->unit, sizeof(out->unit), "%s", u);
            out->has_unit = 1;
            out->product_text[nstart] = '\0';
            sm_trim(out->product_text);
            size_t z = strlen(out->product_text);
            while (z > 0 && (out->product_text[z - 1] == '-' || out->product_text[z - 1] == ':')) {
                out->product_text[--z] = '\0';
            }
            sm_trim(out->product_text);
            return 0;
        }
    }

    /* Pattern 5: Trailing quantity without unit (e.g. "ચોખા ૨" or "Rice - 2") */
    if (!out->has_quantity) {
        L = strlen(out->product_text);
        size_t qs = L;
        while (qs > 0 && isspace((unsigned char)out->product_text[qs - 1])) qs--;
        size_t qend = qs;
        while (qs > 0 && !isspace((unsigned char)out->product_text[qs - 1])) qs--;
        double q_only = 0;
        size_t used_q = 0;
        if (qs < qend && parse_num(out->product_text + qs, &q_only, &used_q) && (qs + used_q) == qend) {
            out->quantity = q_only;
            out->has_quantity = 1;
            out->product_text[qs] = '\0';
            sm_trim(out->product_text);
            size_t z = strlen(out->product_text);
            while (z > 0 && (out->product_text[z - 1] == '-' || out->product_text[z - 1] == ':')) {
                out->product_text[--z] = '\0';
            }
            sm_trim(out->product_text);
        }
    }

    return 0;
}
