// src/op_cat.c
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <sys/types.h>
#include "ops.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char  **paths;      // argv slices
    int     n;          // number of paths
    int     i;          // current path index

    FILE   *cur;        // current open stream
    int     is_stdin;   // reading from stdin?

    char   *buf;        // reusable getline buffer we hand to engine
    size_t  cap;

    // big file buffer for current FILE*
    char   *fbuf;
    size_t  fbuf_sz;
} cat_cfg;

// Parse: cat [FILE ...]
// Consumes args until next token is recognized as an op (so 'fx cat a b cut ...' works).
// If no file given, default to "-" (stdin).
static int cat_parse(int argc, char **argv, int i, void **cfg_out) {
    cat_cfg *c = calloc(1, sizeof *c);
    if (!c) return -1;

    int j = i;
    if (j < argc && (strcmp(argv[j], "cat") == 0 || strcmp(argv[j], "fp_cat") == 0)) j++;

    int start = j;
    while (j < argc && lookup_op(argv[j]) == NULL) j++;
    int count = j - start;

    static char *dash = "-";
    if (count <= 0) { c->paths = &dash; c->n = 1; }
    else { c->paths = &argv[start]; c->n = count; }

    *cfg_out = c;
    return j;
}

static int cat_open_next(cat_cfg *c) {
    if (c->cur) { if (!c->is_stdin) fclose(c->cur); c->cur = NULL; c->is_stdin = 0; }
    if (c->i >= c->n) return 0; // nothing to open

    const char *p = c->paths[c->i++];
    if (strcmp(p, "-") == 0) {
        c->cur = stdin;
        c->is_stdin = 1;
    } else {
        c->cur = fopen(p, "rb");
        c->is_stdin = 0;
        if (!c->cur) return -1;
    }
    // big buffered IO
    if (!c->fbuf) { c->fbuf_sz = FP_BUF_1M; c->fbuf = malloc(c->fbuf_sz); }
    if (c->cur) setvbuf(c->cur, c->fbuf, _IOFBF, c->fbuf_sz);
    return 1;
}

static int cat_produce(void *vcfg, char **linep, size_t *lenp) {
    cat_cfg *c = vcfg;

    for (;;) {
        if (!c->cur) {
            int o = cat_open_next(c);
            if (o <= 0) return 0;      // EOF across all files or error on open?
            if (o < 0) return -1;      // open failure
        }
        ssize_t n = getline(&c->buf, &c->cap, c->cur);
        if (n >= 0) {
            *linep = c->buf; *lenp = (size_t)n;
            return 1;
        }
        if (feof(c->cur)) {
            if (!c->is_stdin) { fclose(c->cur); }
            c->cur = NULL;
            // loop to next file
            continue;
        }
        // read error
        return -1;
    }
}

static void cat_destroy(void *vcfg) {
    cat_cfg *c = vcfg;
    if (!c) return;
    if (c->cur && !c->is_stdin) fclose(c->cur);
    free(c->buf);
    // don't free c->paths; they point into argv or static "-"
    free(c->fbuf);
    free(c);
}

static const OpSpec SPEC = {
    .name="fp_cat", .kind=OP_SRC,
    .parse=cat_parse, .init=NULL,
    .consume=NULL, .produce=cat_produce, .accept=NULL,
    .flush=NULL, .destroy=cat_destroy, .should_stop=NULL
};

const OpSpec *op_cat_spec(){ return &SPEC; }
