// src/op_registry.c
#include "ops.h"
#include <string.h>

typedef struct { const char *alias; const OpSpec *(*spec)(void); } Alias;

static const Alias ALIASES[] = {
    {"fp_cut",  op_cut_spec},  {"cut",  op_cut_spec},
    {"fp_tr",   op_tr_spec},   {"tr",   op_tr_spec},
    {"fp_grep", op_grep_spec}, {"grep", op_grep_spec},
    {"fp_take", op_take_spec}, {"take", op_take_spec},
    {"fp_find", op_find_spec}, {"find", op_find_spec},
    {"fp_cat", op_cat_spec}, {"cat", op_cat_spec},
};

const OpSpec *lookup_op(const char *token) {
    for (size_t i = 0; i < sizeof(ALIASES)/sizeof(ALIASES[0]); i++) {
        if (strcmp(token, ALIASES[i].alias) == 0) return ALIASES[i].spec();
    }
    return NULL;
}
