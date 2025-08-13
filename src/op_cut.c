// src/op_cut.c
#include "ops.h"
#include "util.h"

#include <stdio.h>

typedef struct {
    char delim;              // -d <char>
    fp_fieldset fields;      // -f LIST
    char *outdelim;          // --output-delimiter=STR (default: delim)
    int suppress_no_delim;   // -s
} cut_cfg;

static int cut_parse(int argc, char **argv, int i, void **cfg_out) {
    cut_cfg *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->delim = '\t';

    int j = i;
    if (j < argc && (strcmp(argv[j], "cut") == 0 || strcmp(argv[j], "fp_cut") == 0)) j++;

    for (; j < argc; j++) {
        char *a = argv[j];

        if (lookup_op(a) != NULL) break;        // fx boundary
        if (a[0] != '-') break;                 // no non-option operands in this subset

        // -d CHAR or -d, (attached)
        if (strcmp(a, "-d") == 0) {
            if (++j >= argc) { free(c); return -1; }
            c->delim = argv[j][0];
            continue;
        }
        if (strncmp(a, "-d", 2) == 0 && a[2] != '\0') { c->delim = a[2]; continue; }

        // -f LIST or -fLIST (supports ranges 1,3-5,7-)
        if (strcmp(a, "-f") == 0) {
            if (++j >= argc) { free(c); return -1; }
            if (fp_fieldset_parse(argv[j], &c->fields) < 0) { free(c); return -1; }
            continue;
        }
        if (strncmp(a, "-f", 2) == 0 && a[2] != '\0') {
            if (fp_fieldset_parse(a+2, &c->fields) < 0) { free(c); return -1; }
            continue;
        }

        if (strcmp(a, "-s") == 0) { c->suppress_no_delim = 1; continue; }

        if (strncmp(a, "--output-delimiter=", 20) == 0) {
            c->outdelim = fp_xstrdup(a+20);
            continue;
        }

        break; // unknown -> let fx see next token
    }

    if (c->fields.bits == NULL) { free(c); return -1; }

    if (!c->outdelim) {
        c->outdelim = malloc(2);
        if (!c->outdelim) { fp_fieldset_free(&c->fields); free(c); return -1; }
        c->outdelim[0]=c->delim; c->outdelim[1]='\0';
    }

    *cfg_out = c;
    return j;
}

static int cut_consume(void *vcfg, char **linep, size_t *lenp) {
    cut_cfg *c = vcfg;
    char *s = *linep; size_t n = *lenp;
    // Keep newline strip+restore for easier joining
    int had_nl = (n && s[n-1] == '\n'); if (had_nl) { s[--n] = '\0'; }

    // Detect delimiter presence
    int has_delim = (memchr(s, c->delim, n) != NULL);
    if (!has_delim && c->suppress_no_delim) {
        if (had_nl) s[n++] = '\n';
        *lenp = n; return ENG_DROP;
    }

    // In-place selection: write pointer wp
    char *wp = s;
    size_t field = 1;
    const char *od = c->outdelim; size_t odl = strlen(od);
    int wrote_any = 0;

    char *p = s; char *end = s + n;
    while (p <= end) {
        char *q = (p == end) ? end : memchr(p, c->delim, (size_t)(end - p));
        if (!q) q = end;
        int take = fp_fieldset_has(&c->fields, field);
        if (take) {
            if (wrote_any && odl) { memmove(wp, od, odl); wp += odl; }
            memmove(wp, p, (size_t)(q - p)); wp += (size_t)(q - p);
            wrote_any = 1;
        }
        if (q == end) break;
        p = q + 1;
        field++;
    }
    
    // restore newline
    if (had_nl) *wp++ = '\n';
    *lenp = (size_t)(wp - s);
    return wrote_any ? ENG_OK : ENG_DROP;
}

static void cut_destroy(void *vcfg) {
    cut_cfg *c = vcfg;
    if (!c) return;
    fp_fieldset_free(&c->fields);
    free(c->outdelim);
    free(c);
}

static const OpSpec SPEC = {
    .name="fp_cut", .kind=OP_MAP,
    .parse=cut_parse, .init=NULL,
    .consume=cut_consume, .produce=NULL, .accept=NULL,
    .flush=NULL, .destroy=cut_destroy, .should_stop=NULL
};

const OpSpec *op_cut_spec(){ return &SPEC; }
