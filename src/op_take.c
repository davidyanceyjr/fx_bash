// src/op_take.c
#include "ops.h"
#include "util.h"

typedef struct {
    long n;
    long seen;
} take_cfg;

static int take_parse(int argc, char **argv, int i, void **cfg_out) {
    take_cfg *c = calloc(1, sizeof *c);
    int j = i+1;
    // Accept: -n N  or just N
    if (j < argc && strcmp(argv[j], "-n") == 0 && j+1 < argc) {
        if (fp_parse_long(argv[j+1], &c->n) < 0) { free(c); return -1; }
        j += 2;
    } else if (j < argc) {
        if (fp_parse_long(argv[j], &c->n) < 0) { free(c); return -1; }
        j++;
    } else { free(c); return -1; }
    if (c->n < 0) c->n = 0;
    *cfg_out = c;
    return j;
}

static int take_accept(void *vcfg, const char *line, size_t len) {
    (void)line; (void)len;
    take_cfg *c = vcfg;
    if (fwrite(line, 1, len, stdout) < len) return -1;
    c->seen++;
    return (c->seen >= c->n) ? 0 : 1; // 0 => stop
}
static void take_destroy(void *vcfg){ free(vcfg); }

static const OpSpec SPEC = {
    .name="fp_take", .kind=OP_SINK,
    .parse=take_parse, .init=NULL,
    .consume=NULL, .produce=NULL, .accept=take_accept,
    .flush=NULL, .destroy=take_destroy, .should_stop=NULL
};
const OpSpec *op_take_spec(){ return &SPEC; }
