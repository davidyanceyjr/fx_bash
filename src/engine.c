// src/engine.c
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>

#include "engine.h"
#include "util.h"   // defines FP_BUF_1M normally

// Fallback in case an older util.h is picked up or include order breaks
#ifndef FP_BUF_1M
#define FP_BUF_1M (1<<20)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *in;
    FILE *out;
    char *buf;
    size_t cap;
    size_t len;
    int emitted; // any line reached output/sink
} EngineState;

/*** default stdio source/sink ***/
typedef struct { int dummy; } StdioSrcCfg;
typedef struct { int dummy; } StdioSinkCfg;

static int stdio_src_produce(void *cfg, char **linep, size_t *lenp) {
    (void)cfg;
    static char *buf = NULL;
    static size_t cap = 0;
    ssize_t n = getline(&buf, &cap, stdin);
    if (n < 0) return feof(stdin) ? 0 : -1;
    *linep = buf; *lenp = (size_t)n;
    return 1;
}
static void stdio_src_destroy(void *cfg) { (void)cfg; }

static int stdio_sink_accept(void *cfg, const char *line, size_t len) {
    (void)cfg;
    if (fwrite(line, 1, len, stdout) < len) return -1;
    return 1; // continue
}
static void stdio_sink_destroy(void *cfg) { (void)cfg; }

static const OpSpec STDIO_SRC = {
    .name = "stdin",
    .kind = OP_SRC,
    .parse = NULL, .init = NULL,
    .consume = NULL,
    .produce = stdio_src_produce,
    .accept  = NULL,
    .flush = NULL,
    .destroy = stdio_src_destroy,
    .should_stop = NULL,
};
static const OpSpec STDIO_SINK = {
    .name = "stdout",
    .kind = OP_SINK,
    .parse = NULL, .init = NULL,
    .consume = NULL, .produce = NULL,
    .accept = stdio_sink_accept,
    .flush = NULL,
    .destroy = stdio_sink_destroy,
    .should_stop = NULL,
};

const OpSpec *engine_stdio_source(){ return &STDIO_SRC; }
const OpSpec *engine_stdio_sink(){ return &STDIO_SINK; }

/*** plan helpers ***/
int engine_add_default_stdio_source_sink_if_needed(Plan *p) {
    // Ensure first is SRC; last may be SINK (optional; stdout otherwise).
    int have_src = (p->nsteps > 0 && p->steps[0].spec->kind == OP_SRC);
    if (!have_src) {
        p->steps = realloc(p->steps, sizeof(PlanStep)*(p->nsteps+1));
        if (!p->steps) return -1;
        memmove(&p->steps[1], &p->steps[0], sizeof(PlanStep)*p->nsteps);
        p->steps[0].spec = engine_stdio_source();
        p->steps[0].cfg = NULL;
        p->nsteps++;
    }
    // SINK is optional; we'll use stdout when no sink op at tail.
    return 0;
}

void engine_free_plan(Plan *p) {
    if (!p || !p->steps) return;
    for (int i = 0; i < p->nsteps; i++) {
        if (p->steps[i].spec && p->steps[i].spec->destroy && p->steps[i].cfg)
            p->steps[i].spec->destroy(p->steps[i].cfg);
        p->steps[i].cfg = NULL;
    }
    free(p->steps); p->steps = NULL; p->nsteps = 0;
}

/*** main streaming loop with multi-SOURCE support ***/
int engine_run_plan(Plan *p) {
    // Big stdio buffers
    static char inbuf[FP_BUF_1M], outbuf[FP_BUF_1M];
    setvbuf(stdin,  inbuf,  _IOFBF, sizeof inbuf);
    setvbuf(stdout, outbuf, _IOFBF, sizeof outbuf);

    if (p->nsteps == 0) return 0;

    // init hooks
    for (int i = 0; i < p->nsteps; i++) {
        if (p->steps[i].spec->init) {
            if (p->steps[i].spec->init(p->steps[i].cfg) < 0) {
                return 2;
            }
        }
    }

    // Determine explicit sink and the range of sources
    int tail_is_sink = (p->steps[p->nsteps-1].spec->kind == OP_SINK);

    int src_end = 0;
    while (src_end < p->nsteps && p->steps[src_end].spec->kind == OP_SRC) src_end++;
    if (src_end == 0) {
        // Shouldn't happen because engine_add_default... ensures at least one SRC.
        // But if it does, treat as stdin.
        src_end = 1;
    }

    // streaming
    char *line = NULL;
    size_t len = 0;
    int rc = 0;
    int any_emitted = 0;

    int cur_src = 0;
    for (;;) {
        // Pull from current/next source
        int produced = -1;
        while (cur_src < src_end) {
            const OpSpec *sps = p->steps[cur_src].spec;
            int pr = sps->produce(p->steps[cur_src].cfg, &line, &len);
            if (pr > 0) { produced = 1; break; }
            if (pr < 0) { rc = 2; goto end_stream; }
            // pr == 0 => EOF for this source, advance to next
            cur_src++;
        }
        if (produced <= 0) break; // no more sources => stream done

        // Apply non-source ops in order
        int drop = 0;
        int early_stop = 0;
        for (int i = src_end; i < p->nsteps; i++) {
            const OpSpec *sp = p->steps[i].spec;
            if (sp->kind == OP_MAP || sp->kind == OP_FILTER) {
                int cr = sp->consume(p->steps[i].cfg, &line, &len);
                if (cr < 0) { rc = 2; goto end_stream; }
                if (cr > 0) { drop = 1; break; }
                if (sp->should_stop && sp->should_stop(p->steps[i].cfg)) {
                    early_stop = 1;
                }
            }
        }

        if (!drop) {
            if (tail_is_sink) {
                int ar = p->steps[p->nsteps-1].spec->accept(p->steps[p->nsteps-1].cfg, line, len);
                if (ar < 0) { rc = 2; goto end_stream; }
                if (ar == 0) { any_emitted = 1; goto end_stream; } // sink requested stop
                any_emitted = 1;
            } else {
                if (fwrite(line, 1, len, stdout) < len) { rc = 2; goto end_stream; }
                any_emitted = 1;
            }
        }

        if (early_stop) break; // stop whole stream after handling this record
    }

end_stream:
    // flush hooks
    for (int i = 0; i < p->nsteps; i++) {
        if (p->steps[i].spec->flush) {
            if (p->steps[i].spec->flush(p->steps[i].cfg) < 0) rc = 2;
        }
    }
    // exit code policy: 0 if any emitted, 1 if none (grep-like), else 2 on error
    if (rc >= 2) return rc;
    return any_emitted ? 0 : 1;
}
