// include/engine.h
#ifndef FP_ENGINE_H
#define FP_ENGINE_H

#include <stddef.h>

typedef enum { OP_SRC, OP_MAP, OP_FILTER, OP_SINK } OpKind;

// Status codes used by engine and ops
enum {
    ENG_OK = 0,
    ENG_DROP = 1,     // FILTER/MAP consume() asks to drop the line
    ENG_EOF = 0,      // produce() returning 0 == EOF per spec
    ENG_ERR = -1,
};

typedef struct OpSpec {
    const char *name;
    OpKind kind;

    // Parse argv at index i (argv[i] is this op name or already consumed).
    // Return next index on success (> i), or <0 on error. cfg_out must be set on success.
    int  (*parse)(int argc, char **argv, int i, void **cfg_out);

    // Optional: once per plan. Return 0 on success, <0 on error.
    int  (*init)(void *cfg);

    // MAP/FILTER: consume one line. May modify *linep in place (preferred).
    // Return: 0 => emit (len may change), >0 => drop (ENG_DROP), <0 => error.
    int  (*consume)(void *cfg, char **linep, size_t *lenp);

    // SOURCE: produce one line into *linep/*lenp.
    // Return: 1 => produced, 0 => EOF, <0 => error.
    int  (*produce)(void *cfg, char **linep, size_t *lenp);

    // SINK: accept a line. Return 0 => stop streaming, >0 => continue, <0 => error.
    int  (*accept)(void *cfg, const char *line, size_t len);

    // Optional end-of-stream hook.
    int  (*flush)(void *cfg);

    // Free cfg
    void (*destroy)(void *cfg);

    // Optional: hint engine to stop early (e.g., grep -m N). Return nonzero to stop.
    int  (*should_stop)(void *cfg);
} OpSpec;

// A compiled plan step
typedef struct {
    const OpSpec *spec;
    void *cfg;
} PlanStep;

typedef struct {
    PlanStep *steps;
    int       nsteps;
} Plan;

// Public engine API
int engine_run_plan(Plan *p); // returns exit code (0 ok, 1 no matches, >=2 errors)
int engine_add_default_stdio_source_sink_if_needed(Plan *p);

// Helper to free a plan (calls destroy on cfgs).
void engine_free_plan(Plan *p);

// Default stdin-line source and stdout sink OpSpecs
const OpSpec *engine_stdio_source();
const OpSpec *engine_stdio_sink();

#endif // FP_ENGINE_H
