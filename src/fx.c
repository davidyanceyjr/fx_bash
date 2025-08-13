// src/fx.c
#include "engine.h"
#include "ops.h"
#include "util.h"

#include <builtins.h>
#include <shell.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fx_build_plan(int argc, char **argv, Plan *plan, const char *who) {
    // argv[0] == "fx"; subsequent tokens are op names with args
    plan->steps = NULL; plan->nsteps = 0;
    
    // bash builtins' WORD_LIST omits the builtin name; argv[0] is the first token (e.g., "cut")
    int i = 0;

    while (i < argc) {
        const char *tok = argv[i];
        const OpSpec *op = lookup_op(tok);
        if (!op) {
            fp_errf(who, -1, "", "unknown op '%s'\n", tok);
            return -1;
        }
        void *cfg = NULL;
        int next = op->parse(argc, argv, i, &cfg);
        if (next <= i) {
            if (cfg && op->destroy) op->destroy(cfg);
            fp_errf(who, -1, op->name, "bad args near '%s'\n", tok);
            return -1;
        }
        plan->steps = realloc(plan->steps, sizeof(PlanStep)*(plan->nsteps+1));
        if (!plan->steps) { if (cfg && op->destroy) op->destroy(cfg); return -1; }
        plan->steps[plan->nsteps].spec = op;
        plan->steps[plan->nsteps].cfg  = cfg;
        plan->nsteps++;
        i = next;
    }
    if (engine_add_default_stdio_source_sink_if_needed(plan) < 0) return -1;
    return 0;
}

/*** Builtin glue for fx ***/
static int fx_entry(int argc, char **argv) {
    Plan plan = {0};
    if (fx_build_plan(argc, argv, &plan, "fx") < 0) {
        engine_free_plan(&plan);
        return EXECUTION_FAILURE; // Bash builtin failure
    }
    int rc = engine_run_plan(&plan);
    engine_free_plan(&plan);
    // Map engine exit status to builtin return:
    // 0 -> EXECUTION_SUCCESS, 1 -> 1, >=2 -> 2
    if (rc == 0) return EXECUTION_SUCCESS;
    if (rc == 1) return 1;
    return 2;
}

/*** Standalone wrappers: each builds plan of [src] -> op -> [maybe sink] ***/
static int run_singleton(const OpSpec *spec, int argc, char **argv, const char *who) {
    Plan plan = {0};
    void *cfg = NULL;
    int next = spec->parse(argc, argv, 0, &cfg);
    if (next <= 0 || next != argc) {
        if (cfg && spec->destroy) spec->destroy(cfg);
        fp_errf(who, -1, spec->name, "usage error\n");
        return 2;
    }
    plan.steps = malloc(sizeof(PlanStep));
    if (!plan.steps) { if (cfg && spec->destroy) spec->destroy(cfg); return 2; }
    plan.steps[0].spec = spec;
    plan.steps[0].cfg  = cfg;
    plan.nsteps = 1;
    if (engine_add_default_stdio_source_sink_if_needed(&plan) < 0) {
        engine_free_plan(&plan); return 2;
    }
    int rc = engine_run_plan(&plan);
    engine_free_plan(&plan);
    if (rc == 0) return EXECUTION_SUCCESS;
    if (rc == 1) return 1;
    return 2;
}

/*** Bash builtin declarations ***/

// fx
int fx_builtin(WORD_LIST *list) {
    // Convert WORD_LIST to argc/argv
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = fx_entry(argc, argv);
    free(argv);
    return rc;
}

static char *fx_doc[] = {
    "fx: fused pipeline of ops (cut/tr/grep/take/find-stub)",
    "Usage: fx <op args>...",
    NULL
};

struct builtin fx_struct = {
    .name = "fx",
    .function = fx_builtin,
    .flags = BUILTIN_ENABLED,
    .long_doc = fx_doc,
    .short_doc = "fx <ops...>",
    .handle = 0
};

/*** Standalone builtin fronts ***/
int fp_cut_builtin(WORD_LIST *list) {
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = run_singleton(op_cut_spec(), argc, argv, "fp_cut");
    free(argv); return rc;
}
int fp_tr_builtin(WORD_LIST *list) {
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = run_singleton(op_tr_spec(), argc, argv, "fp_tr");
    free(argv); return rc;
}
int fp_grep_builtin(WORD_LIST *list) {
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = run_singleton(op_grep_spec(), argc, argv, "fp_grep");
    free(argv); return rc;
}
int fp_take_builtin(WORD_LIST *list) {
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = run_singleton(op_take_spec(), argc, argv, "fp_take");
    free(argv); return rc;
}
int fp_find_builtin(WORD_LIST *list) {
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = run_singleton(op_find_spec(), argc, argv, "fp_find");
    free(argv); return rc;
}

int fp_emit_builtin(WORD_LIST *list) {
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = run_singleton(op_emit_spec(), argc, argv, "fp_emit");
    free(argv); return rc;
}


int fp_cat_builtin(WORD_LIST *list) {
    int argc = 0; for (WORD_LIST *w = list; w; w = w->next) argc++;
    char **argv = calloc(argc+1, sizeof(char*));
    int i = 0; for (WORD_LIST *w = list; w; w = w->next) argv[i++] = w->word->word;
    int rc = run_singleton(op_cat_spec(), argc, argv, "fp_cat");
    free(argv); return rc;
}

static char *cat_doc[] = { "fp_cat: cat-like source", NULL };
static char *emit_doc[] = { "fp_emit: emit one line from an argument", NULL };
static char *cut_doc[]  = { "fp_cut: cut-like filter", NULL };
static char *tr_doc[]   = { "fp_tr: tr-like transliteration", NULL };
static char *grep_doc[] = { "fp_grep: grep-like filter", NULL };
static char *take_doc[] = { "fp_take: head -n N sink", NULL };
static char *find_doc[] = { "fp_find: SOURCE stub", NULL };

struct builtin fp_emit_struct = { "fp_emit", fp_emit_builtin, BUILTIN_ENABLED, emit_doc, "fp_emit STR", 0 };
struct builtin fp_cat_struct = { "fp_cat", fp_cat_builtin, BUILTIN_ENABLED, cat_doc, "fp_cat [FILE...]", 0 };
struct builtin fp_cut_struct  = { "fp_cut",  fp_cut_builtin,  BUILTIN_ENABLED, cut_doc,  "fp_cut [opts]",  0 };
struct builtin fp_tr_struct   = { "fp_tr",   fp_tr_builtin,   BUILTIN_ENABLED, tr_doc,   "fp_tr [opts]",   0 };
struct builtin fp_grep_struct = { "fp_grep", fp_grep_builtin, BUILTIN_ENABLED, grep_doc, "fp_grep [opts]", 0 };
struct builtin fp_take_struct = { "fp_take", fp_take_builtin, BUILTIN_ENABLED, take_doc, "fp_take [opts]", 0 };
struct builtin fp_find_struct = { "fp_find", fp_find_builtin, BUILTIN_ENABLED, find_doc, "fp_find [opts]", 0 };

/* Export table for all builtins in this module */
struct builtin *builtins[] = {
    &fx_struct,
    &fp_cat_struct,
    &fp_emit_struct,
    &fp_cut_struct,
    &fp_tr_struct,
    &fp_grep_struct,
    &fp_take_struct,
    &fp_find_struct,
    0   /* Must be NULL-terminated */
};
