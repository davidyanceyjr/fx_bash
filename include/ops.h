// include/ops.h
#ifndef FP_OPS_H
#define FP_OPS_H

#include "engine.h"

// Registry
const OpSpec *lookup_op(const char *token);

// Exposed ops
const OpSpec *op_cut_spec();
const OpSpec *op_tr_spec();
const OpSpec *op_grep_spec();
const OpSpec *op_take_spec();
const OpSpec *op_find_spec(); // SOURCE stub
const OpSpec *op_emit_spec();  // SOURCE: emit lines from argv
const OpSpec *op_cat_spec();  // SOURCE: cat like file reader

#endif // FP_OPS_H
