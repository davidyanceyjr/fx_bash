// src/op_emit.c
#include "ops.h"
#include "util.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    char  **items;
    int     n, i;
    char   *buf;
    size_t  cap;
} emit_cfg;

static int emit_parse(int argc, char **argv, int i, void **cfg_out) {
    // argv[i] is "emit" or "fp_emit"
    int j = i + 1;
    if (j >= argc) return -1;

    // Collect args until next token is a known op OR end of argv
    int start = j;
    while (j < argc) {
        if (lookup_op(argv[j]) != NULL) break; // stop before next op
        j++;
    }
    int count = j - start;
    if (count <= 0) return -1;

    emit_cfg *c = calloc(1, sizeof *c);
    c->items = &argv[start];
    c->n = count;
    *cfg_out = c;
    return j; // next index after our args
}

static int emit_produce(void *vcfg, char **linep, size_t *lenp) {
    emit_cfg *c = vcfg;
    if (c->i >= c->n) return 0;
    const char *s = c->items[c->i++];
    size_t n = strlen(s);
    size_t need = n + (n && s[n-1]=='\n' ? 0 : 1);
    if (need+1 > c->cap) { c->cap = (need+1)*2; c->buf = realloc(c->buf, c->cap); }
    memcpy(c->buf, s, n);
    if (n==0 || s[n-1] != '\n') c->buf[n++] = '\n';
    c->buf[n] = '\0';
    *linep = c->buf; *lenp = n;
    return 1;
}

static void emit_destroy(void *vcfg) {
    emit_cfg *c = vcfg;
    if (!c) return;
    free(c->buf);
    free(c);
}

static const OpSpec SPEC = {
    .name="fp_emit", .kind=OP_SRC,
    .parse=emit_parse, .init=NULL,
    .consume=NULL, .produce=emit_produce, .accept=NULL,
    .flush=NULL, .destroy=emit_destroy, .should_stop=NULL
};
const OpSpec *op_emit_spec(){ return &SPEC; }
