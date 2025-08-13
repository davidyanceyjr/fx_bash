// src/op_grep.c
#include "ops.h"
#include "util.h"
#include <string.h>

typedef struct {
    fp_grepspec g;
} grep_cfg;

static int grep_parse(int argc, char **argv, int i, void **cfg_out) {
    grep_cfg *c = calloc(1, sizeof *c);
    if (!c) return -1;
    int j = i;

    if (j < argc && (strcmp(argv[j], "grep") == 0 || strcmp(argv[j], "fp_grep") == 0)) j++;

    int ext=0, fixed=0, icase=0, invert=0; long maxm=0;
    while (j < argc && argv[j][0] == '-') {
        if (lookup_op(argv[j]) != NULL) break;
        if (strcmp(argv[j], "-E") == 0) { ext=1; j++; continue; }
        if (strcmp(argv[j], "-F") == 0) { fixed=1; j++; continue; }
        if (strcmp(argv[j], "-i") == 0) { icase=1; j++; continue; }
        if (strcmp(argv[j], "-v") == 0) { invert=1; j++; continue; }
        if (strcmp(argv[j], "-m") == 0) {
            if (++j >= argc) { free(c); return -1; }
            if (fp_parse_long(argv[j], &maxm) < 0) { free(c); return -1; }
            j++; continue;
        }
        break;
    }

    if (j >= argc || lookup_op(argv[j]) != NULL) { free(c); return -1; }
    const char *pattern = argv[j++];

    if (fp_grepspec_compile(&c->g, ext, fixed, icase, pattern) < 0) { free(c); return -1; }
    c->g.invert = invert;
    c->g.max_matches = maxm;
    *cfg_out = c;
    return j;
}

static int grep_consume(void *vcfg, char **linep, size_t *lenp) {
    grep_cfg *c = vcfg;
    int m = fp_grepspec_match_line(&c->g, *linep, *lenp);
    if (c->g.invert) m = !m;
    if (m) {
        if (c->g.max_matches > 0 && ++c->g.matched >= c->g.max_matches) {
            // emit this line, then engine will see should_stop() and end
        }
        return ENG_OK;
    }
    return ENG_DROP;
}
static int grep_should_stop(void *vcfg) {
    grep_cfg *c = vcfg;
    return (c->g.max_matches > 0 && c->g.matched >= c->g.max_matches);
}
static void grep_destroy(void *vcfg){ fp_grepspec_free(&((grep_cfg*)vcfg)->g); free(vcfg); }

static const OpSpec SPEC = {
    .name="fp_grep", .kind=OP_FILTER,
    .parse=grep_parse, .init=NULL,
    .consume=grep_consume, .produce=NULL, .accept=NULL,
    .flush=NULL, .destroy=grep_destroy, .should_stop=grep_should_stop
};
const OpSpec *op_grep_spec(){ return &SPEC; }
