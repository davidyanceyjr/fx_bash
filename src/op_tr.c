// src/op_tr.c
#define _POSIX_C_SOURCE 200809L
#include "ops.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    fp_trspec t;
} tr_cfg;

/* ---- forward decls so SPEC can reference them ---- */
static int  tr_parse(int argc, char **argv, int i, void **cfg_out);
static int  tr_consume(void *cfg, char **linep, size_t *lenp);
static void tr_destroy(void *cfg);

/* ---- OpSpec ---- */
static const OpSpec SPEC = {
    .name="fp_tr", .kind=OP_MAP,
    .parse=tr_parse, .init=NULL,
    .consume=tr_consume, .produce=NULL, .accept=NULL,
    .flush=NULL, .destroy=tr_destroy, .should_stop=NULL
};
const OpSpec *op_tr_spec(void){ return &SPEC; }

/* ---- parser: works in standalone and inside fx ---- */
static int tr_parse(int argc, char **argv, int i, void **cfg_out) {
    tr_cfg *c = calloc(1, sizeof *c);
    if (!c) return -1;
    int j = i;

    /* optional op token so both "fp_tr ..." and "tr ..." work */
    if (j < argc && (strcmp(argv[j], "tr") == 0 || strcmp(argv[j], "fp_tr") == 0)) j++;

    /* flags */
    while (j < argc && argv[j][0] == '-') {
        if (lookup_op(argv[j]) != NULL) break;   // fx boundary
        if (strcmp(argv[j], "-d") == 0) { c->t.delete_mode  = 1; j++; continue; }
        if (strcmp(argv[j], "-s") == 0) { c->t.squeeze_mode = 1; j++; continue; }
        break;
    }

    if (j >= argc) { free(c); return -1; }
    const char *set1 = argv[j++];
    const char *set2 = c->t.delete_mode ? "" :
                       (j < argc && lookup_op(argv[j]) == NULL ? argv[j++] : "");

    if (fp_trspec_build(&c->t, set1, set2) < 0) { free(c); return -1; }
    *cfg_out = c;
    return j;
}

/* ---- consume: in-place transliteration ---- */
static int tr_consume(void *vcfg, char **linep, size_t *lenp) {
    tr_cfg *c = vcfg;
    fp_tr_inplace(linep, lenp, &c->t);
    return 0; /* emit */
}

static void tr_destroy(void *vcfg) {
    tr_cfg *c = vcfg;
    free(c);
}
