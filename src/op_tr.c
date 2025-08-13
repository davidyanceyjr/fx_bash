// src/op_tr.c
#include "ops.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    fp_trspec t;
} tr_cfg;

static int tr_parse(int argc, char **argv, int i, void **cfg_out) {
    tr_cfg *c = calloc(1, sizeof *c);
    int j = i+1;
    int del = 0, squz = 0;
    while (j < argc && argv[j][0] == '-') {
        if (strcmp(argv[j], "-d") == 0) { del = 1; j++; continue; }
        if (strcmp(argv[j], "-s") == 0) { squz = 1; j++; continue; }
        break;
    }
    if (j+ (del?1:2) -1 >= argc) { free(c); return -1; }
    const char *set1 = argv[j++];
    const char *set2 = del ? "" : (j < argc ? argv[j++] : "");
    c->t.delete_mode = del;
    c->t.squeeze_mode = squz;
    if (fp_trspec_build(&c->t, set1, set2) < 0) { free(c); return -1; }
    *cfg_out = c;
    return j;
}

static int tr_consume(void *vcfg, char **linep, size_t *lenp) {
    tr_cfg *c = vcfg;
    fp_tr_inplace(linep, lenp, &c->t);
    return ENG_OK;
}
static void tr_destroy(void *vcfg){ free(vcfg); }

static const OpSpec SPEC = {
    .name="fp_tr", .kind=OP_MAP,
    .parse=tr_parse, .init=NULL,
    .consume=tr_consume, .produce=NULL, .accept=NULL,
    .flush=NULL, .destroy=tr_destroy, .should_stop=NULL
};
const OpSpec *op_tr_spec(){ return &SPEC; }
