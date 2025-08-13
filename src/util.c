#define _POSIX_C_SOURCE 200809L
#include "util.h"

#include <sys/types.h>

/* printf-like error helper */
void fp_errf(const char *who, int k, const char *name, const char *fmt, ...) {
    fprintf(stderr, "%s: ", who);
    if (k >= 0) {
        fprintf(stderr, "op#%d", k);
        if (name && *name)
            fprintf(stderr, " (%s)", name);
        fputs(": ", stderr);
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* strdup with fatal on OOM */
char *fp_xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char*)malloc(n+1);
    if (!p) {
        fprintf(stderr, "fatal: out of memory in fp_xstrdup\n");
        exit(2);
    }
    memcpy(p, s, n+1);
    return p;
}

/* Parse decimal integer into long */
int fp_parse_long(const char *s, long *out) {
    char *e = NULL;
    errno = 0;
    long v = strtol(s, &e, 10);
    if (errno || e == s || *e != '\0') return -1;
    *out = v;
    return 0;
}

/* ---------------- Fieldset (bitset) ---------------- */

static void fs_set(fp_fieldset *fs, size_t idx1) {
    if (idx1 == 0) return;
    size_t bit = idx1 - 1;
    size_t need_bytes = bit/8 + 1;
    size_t have_bytes = fs->nbits ? (fs->nbits + 7)/8 : 0;
    if (need_bytes > have_bytes) {
        size_t new_bytes = have_bytes ? have_bytes : 32;
        while (new_bytes < need_bytes) new_bytes *= 2;
        uint8_t *nb = (uint8_t*)realloc(fs->bits, new_bytes);
        if (!nb) return;
        if (new_bytes > have_bytes) memset(nb + have_bytes, 0, new_bytes - have_bytes);
        fs->bits = nb;
        fs->nbits = new_bytes * 8;
    }
    fs->bits[bit/8] |= (uint8_t)(1u << (bit%8));
}

int fp_fieldset_parse(const char *list, fp_fieldset *fs) {
    memset(fs, 0, sizeof *fs);
    const char *p = list;
    while (*p) {
        char *e = NULL;
        long a = strtol(p, &e, 10);
        if (e == p || a <= 0) { fp_fieldset_free(fs); return -1; }
        long b = a;
        p = e;
        if (*p == '-') {
            p++;
            if (*p == '\0' || *p == ',') { /* open range a- => cap at 4096 for scaffold */
                b = 4096;
                fs->has_ranges = 1;
            } else {
                long t = strtol(p, &e, 10);
                if (e == p || t < a) { fp_fieldset_free(fs); return -1; }
                b = t; p = e; fs->has_ranges = 1;
            }
        }
        for (long k = a; k <= b; k++) fs_set(fs, (size_t)k);
        if (*p == ',') { p++; continue; }
        if (*p == '\0') break;
        fp_fieldset_free(fs); return -1;
    }
    /* ensure at least some allocation for has() bounds */
    if (!fs->bits) {
        fs->bits = (uint8_t*)calloc(1, 32);
        fs->nbits = 32*8;
    }
    return 0;
}

int fp_fieldset_has(fp_fieldset *fs, size_t idx1) {
    if (idx1 == 0) return 0;
    size_t bit = idx1 - 1;
    if (bit >= fs->nbits) return 0;
    return !!(fs->bits[bit/8] & (uint8_t)(1u << (bit%8)));
}

void fp_fieldset_free(fp_fieldset *fs) {
    free(fs->bits);
    fs->bits = NULL;
    fs->nbits = 0;
    fs->has_ranges = 0;
}

/* ---------------- Your field-list (kept) ---------------- */

int fp_parse_field_list(const char *s, fp_field_list *out) {
    char *dup = strdup(s);
    if (!dup) return -1;
    out->ranges = NULL;
    out->nranges = 0;

    char *tok, *save;
    for (tok = strtok_r(dup, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        fp_field_range r;
        char *dash = strchr(tok, '-');
        if (dash) {
            *dash = '\0';
            long a, b;
            if (fp_parse_long(tok, &a) < 0 || fp_parse_long(dash + 1, &b) < 0) { free(dup); return -1; }
            r.start = (int)a; r.end = (int)b;
        } else {
            long a;
            if (fp_parse_long(tok, &a) < 0) { free(dup); return -1; }
            r.start = r.end = (int)a;
        }
        fp_field_range *tmp = (fp_field_range*)realloc(out->ranges, (out->nranges + 1) * sizeof(fp_field_range));
        if (!tmp) { free(out->ranges); free(dup); return -1; }
        out->ranges = tmp;
        out->ranges[out->nranges++] = r;
    }
    free(dup);
    return 0;
}

void fp_free_field_list(fp_field_list *fl) {
    free(fl->ranges);
    fl->ranges = NULL;
    fl->nranges = 0;
}

/* ---------------- Regex/fixed wrapper (yours) ---------------- */

int fp_regex_compile(fp_regex *r, const char *pat, int extended, int icase, int fixed) {
    memset(r, 0, sizeof(*r));
    r->is_fixed = fixed;
    r->icase = icase;
    if (fixed) {
        r->fixed_pat = strdup(pat);
        return r->fixed_pat ? 0 : -1;
    }
    int cflags = REG_NOSUB | REG_NEWLINE;
    if (extended) cflags |= REG_EXTENDED;
    if (icase)    cflags |= REG_ICASE;
    return regcomp(&r->rx, pat, cflags);
}

/* naive ASCII memmem with optional casefold */
static int ascii_memmem_case(const char *h, size_t hl, const char *n, size_t nl, int icase) {
    if (nl == 0) return 1;
    if (!icase) {
        const char *p = h;
        while (1) {
            const void *q = memchr(p, n[0], (size_t)(h + hl - p));
            if (!q) return 0;
            size_t off = (const char*)q - h;
            if (off + nl <= hl && memcmp(h + off, n, nl) == 0) return 1;
            p = (const char*)q + 1;
        }
    } else {
        for (size_t i = 0; i + nl <= hl; i++) {
            size_t j = 0;
            for (; j < nl; j++) {
                int a = fp_ascii_tolower((unsigned char)h[i+j]);
                int b = fp_ascii_tolower((unsigned char)n[j]);
                if (a != b) break;
            }
            if (j == nl) return 1;
        }
        return 0;
    }
}

int fp_regex_match(fp_regex *r, const char *s, size_t len) {
    if (r->is_fixed) {
        size_t patlen = strlen(r->fixed_pat);
        return ascii_memmem_case(s, len, r->fixed_pat, patlen, r->icase);
    }
    return regexec(&r->rx, s, 0, NULL, 0) == 0;
}

void fp_regex_free(fp_regex *r) {
    if (r->is_fixed) {
        free(r->fixed_pat);
        r->fixed_pat = NULL;
    } else {
        regfree(&r->rx);
    }
}

/* ---------------- tr (transliteration) ---------------- */

/* fill selected[] for POSIX character classes we support */
static void tr_fill_class(unsigned char sel[256], const char *name) {
    if (strcmp(name, "lower") == 0) {
        for (int c = 'a'; c <= 'z'; c++) sel[(unsigned char)c] = 1;
    } else if (strcmp(name, "upper") == 0) {
        for (int c = 'A'; c <= 'Z'; c++) sel[(unsigned char)c] = 1;
    }
}

/* parse a SET into selected[]; supports a-b ranges and [:class:] (lower/upper) */
static int tr_parse_set(unsigned char sel[256], const char *set) {
    memset(sel, 0, 256);
    const char *p = set;
    while (*p) {
        if (p[0] == '[' && p[1] == ':') {
            const char *q = strstr(p, ":]");
            if (!q) return -1;
            char cls[16] = {0};
            size_t n = (size_t)(q - (p + 2));
            if (n == 0 || n >= sizeof cls) return -1;
            memcpy(cls, p + 2, n);
            tr_fill_class(sel, cls);
            p = q + 2;
            continue;
        }
        if (p[1] == '-' && p[2]) {
            unsigned char a = (unsigned char)p[0];
            unsigned char b = (unsigned char)p[2];
            if (a <= b) for (int c = a; c <= b; c++) sel[(unsigned char)c] = 1;
            p += 3;
            continue;
        }
        sel[(unsigned char)*p++] = 1;
    }
    return 0;
}

int fp_trspec_build(fp_trspec *t, const char *set1, const char *set2) {
    unsigned char s1[256] = {0}, s2sel[256] = {0};
    if (tr_parse_set(s1, set1) < 0) return -1;

    /* identity map + clear selection by default */
    for (int i = 0; i < 256; i++) {
        t->map[i] = (unsigned char)i;
        t->selected[i] = 0;
    }

    if (t->delete_mode) {
        /* deletion: mark membership only */
        for (int i = 0; i < 256; i++) if (s1[i]) t->selected[i] = 1;
        return 0;
    }

    /* build mapping from s1 to s2 (pad with last char if s2 shorter) */
    if (tr_parse_set(s2sel, set2) < 0) return -1;
    unsigned char seq[256]; size_t sn = 0;
    for (int i = 0; i < 256; i++) if (s2sel[i]) seq[sn++] = (unsigned char)i;
    if (sn == 0) { seq[0] = 0; sn = 1; } /* degenerate: map to NUL (rare, but defined) */

    size_t k = 0;
    for (int i = 0; i < 256; i++) {
        if (s1[i]) {
            t->map[i] = seq[(k < sn) ? k : (sn - 1)];
            t->selected[i] = 1;
            k++;
        }
    }
    return 0;
}

void fp_tr_inplace(char **linep, size_t *lenp, const fp_trspec *t) {
    char  *s  = *linep;
    size_t n  = *lenp;
    char  *wp = s;

    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];

        if (t->delete_mode) {
            if (t->selected[ch]) continue; /* drop */
            if (t->squeeze_mode && wp > s && (unsigned char)*(wp-1) == ch && t->selected[ch]) continue;
            *wp++ = (char)ch;
        } else {
            unsigned char out = t->selected[ch] ? t->map[ch] : ch;
            if (t->squeeze_mode && wp > s && (unsigned char)*(wp-1) == out && t->selected[ch]) continue;
            *wp++ = (char)out;
        }
    }
    *lenp = (size_t)(wp - s);
}
/* ---------------- Grep-spec shim used by op_grep ---------------- */

int fp_grepspec_compile(fp_grepspec *g, int extended, int fixed, int ignore_case, const char *pattern) {
    memset(g, 0, sizeof *g);
    g->use_regex  = !fixed;
    g->ignore_case = ignore_case;
    g->fixed = fixed ? fp_xstrdup(pattern) : NULL;
    g->fixed_len = fixed ? strlen(pattern) : 0;
    /* compile underlying wrapper */
    if (fp_regex_compile(&g->rewrap, pattern, extended, ignore_case, fixed) < 0) {
        free(g->fixed); g->fixed = NULL; g->fixed_len = 0;
        return -1;
    }
    return 0;
}

int fp_grepspec_match_line(fp_grepspec *g, const char *s, size_t len) {
    /* Use your wrapper */
    return fp_regex_match(&g->rewrap, s, len) ? 1 : 0;
}

void fp_grepspec_free(fp_grepspec *g) {
    fp_regex_free(&g->rewrap);
    free(g->fixed); g->fixed = NULL; g->fixed_len = 0;
}
