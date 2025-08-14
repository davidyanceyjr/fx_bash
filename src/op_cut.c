// src/op_cut.c
#define _POSIX_C_SOURCE 200809L
#include "ops.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*** config ***/
typedef struct {
    char delim;              // -d <char>
    fp_fieldset fields;      // -f LIST
    char *outdelim;          // --output-delimiter=STR (default: delim as 1-char)
    size_t outdelim_len;
    int suppress_no_delim;   // -s
} cut_cfg;

/*** fwd decls ***/
static int  cut_parse(int argc, char **argv, int i, void **cfg_out);
static int  cut_consume(void *cfg, char **linep, size_t *lenp);
static void cut_destroy(void *cfg);

/*** OpSpec ***/
static const OpSpec SPEC = {
    .name="fp_cut", .kind=OP_MAP,
    .parse=cut_parse, .init=NULL,
    .consume=cut_consume, .produce=NULL, .accept=NULL,
    .flush=NULL, .destroy=cut_destroy, .should_stop=NULL
};

const OpSpec *op_cut_spec(void){ return &SPEC; }

/*** parser: dual-mode (standalone + fx) ***/
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

        // -f LIST or -fLIST (ranges 1,3-5,7- are supported by fp_fieldset_parse)
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
    c->outdelim_len = strlen(c->outdelim);

    *cfg_out = c;
    return j;
}

/*** in-place cutter
     - scans once, copies selected fields to front
     - preserves trailing '\n' if present
     - if no delimiter found and -s, drop line (return >0)
***/
static int cut_consume(void *vcfg, char **linep, size_t *lenp) {
    cut_cfg *c = vcfg;
    char *s = *linep;
    size_t n = *lenp;

    int has_nl = (n > 0 && s[n-1] == '\n');
    size_t content_len = has_nl ? n-1 : n;
    if (content_len == 0) return 0; // empty line, emit as-is

    char d = c->delim;
    size_t wp = 0;               // write pointer in s[0..content_len]
    int first_out = 1;           // whether we've emitted any field yet
    size_t field_no = 1;
    size_t i = 0;
    int saw_delim = 0;

    while (i <= content_len) {
        size_t start = i;
        // find end of field or end-of-line
        while (i < content_len && s[i] != d) i++;
        size_t end = i; // [start, end)

        if (i < content_len && s[i] == d) { saw_delim = 1; }

        // selected?
        if (fp_fieldset_has(&c->fields, field_no)) {
            if (!first_out) {
                // insert output delimiter
                if (c->outdelim_len == 1) {
                    s[wp++] = c->outdelim[0];
                } else {
                    // need to grow? allocate a new buffer once if needed
                    size_t need = wp + c->outdelim_len + (end - start) + (has_nl ? 1 : 0);
                    if (need > n) {
                        char *nb = realloc(s, need * 2);
                        if (!nb) return -1;
                        *linep = s = nb;
                        *lenp = n = need * 2;
                    }
                    memcpy(s + wp, c->outdelim, c->outdelim_len);
                    wp += c->outdelim_len;
                }
            }
            // copy field bytes
            size_t flen = end - start;
            if (wp + flen + (has_nl ? 1 : 0) > n) {
                size_t need = wp + flen + (has_nl ? 1 : 0);
                char *nb = realloc(s, need * 2);
                if (!nb) return -1;
                *linep = s = nb;
                *lenp = n = need * 2;
            }
            if (flen) memcpy(s + wp, s + start, flen);
            wp += flen;
            first_out = 0;
        }

        // skip delimiter (if any)
        if (i < content_len && s[i] == d) i++;
        field_no++;
    }

    if (!saw_delim && c->suppress_no_delim) {
        // drop record
        return 1; // FILTER drop
    }

    // restore newline
    if (has_nl) s[wp++] = '\n';

    *lenp = wp;
    return 0; // emit
}

static void cut_destroy(void *vcfg) {
    cut_cfg *c = vcfg;
    if (!c) return;
    fp_fieldset_free(&c->fields);
    free(c->outdelim);
    free(c);
}
