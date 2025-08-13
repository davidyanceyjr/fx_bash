# fp_prelude: Bash Loadable Module (fx + fp_cat/fp_emit/fp_cut/fp_tr/fp_grep/fp_take/fp_find)

A production-lean scaffold for a fused streaming engine and a set of source/map/filter/sink ops.

## Included Builtins

### Super-builtin
- **`fx`** — parses a sequence of familiar op tokens (`cat`, `cut`, `tr`, `grep`, `take`, etc.) and runs them in a **fused, single-process pipeline**.

### Standalone Builtins
- **Sources**  
  - `fp_cat` — stream file(s) or stdin, line-by-line (aliased as `cat` inside `fx`).  
  - `fp_emit` — emit literal records given as arguments (aliased as `emit` inside `fx`).
  - `fp_find` — *stub* SOURCE, argument parsing implemented but `produce()` returns “not yet implemented”.
- **Filters / Maps**  
  - `fp_cut` — field extraction (`-d <char>`, `-f LIST`, `--output-delimiter=STR`, `-s`).  
  - `fp_tr` — transliteration (`SET1` `SET2`, `-d`, `-s`, ASCII only, supports `[:lower:]` / `[:upper:]`).  
  - `fp_grep` — grep-like (`-E`, `-F`, `-i`, `-v`, `-m N`).
- **Sinks**  
  - `fp_take` — like `head -n N` for lines; short-circuits the engine.

All ops are available **standalone** or as tokens in `fx` (with or without the `fp_` prefix).

---

## Build

Requires Bash dev headers (e.g., `/usr/include/bash` providing `builtins.h` and `shell.h`).

```bash
make
