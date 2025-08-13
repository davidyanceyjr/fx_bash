// src/op_find.c
#include "ops.h"
#include "util.h"

#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fnmatch.h>
#include <limits.h>

typedef struct {
    char *path;
    DIR  *dir;
    int   depth;
} Frame;

typedef struct {
    char *start;     // default "."
    char  type;      // 0, 'f', 'd'
    char *namepat;   // glob pattern (fnmatch on basename)
    int   maxdepth;  // -1 => unlimited
    int   print0;    // -print0

    // traversal stack (LIFO), iterative to avoid recursion
    Frame *stk;
    int    top;      // points to next free slot
    int    cap;

    // output buffer reused
    char  *obuf;
    size_t ocap;

    // initial node processed?
    int    started;
    int    done;
} find_cfg;

static int push(find_cfg *c, const char *path, int depth, DIR *opendir_now) {
    if (c->top == c->cap) {
        int ncap = c->cap ? c->cap * 2 : 32;
        Frame *n = realloc(c->stk, ncap * sizeof *n);
        if (!n) return -1;
        c->stk = n; c->cap = ncap;
    }
    c->stk[c->top].path = fp_xstrdup(path);
    c->stk[c->top].depth = depth;
    c->stk[c->top].dir = NULL;
    if (opendir_now) c->stk[c->top].dir = opendir_now; // (unused path)
    c->top++;
    return 0;
}

static void pop(find_cfg *c) {
    if (c->top == 0) return;
    c->top--;
    if (c->stk[c->top].dir) closedir(c->stk[c->top].dir);
    free(c->stk[c->top].path);
    c->stk[c->top].path = NULL;
    c->stk[c->top].dir = NULL;
}

static inline const char *basename_c(const char *p) {
    const char *slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
}

static int match_filters(find_cfg *c, const char *path, const struct stat *st, int is_dir) {
    if (c->type == 'f' && S_ISREG(st->st_mode) == 0) return 0;
    if (c->type == 'd' && S_ISDIR(st->st_mode) == 0) return 0;
    if (c->namepat) {
        const char *base = basename_c(path);
        if (fnmatch(c->namepat, base, 0) != 0) return 0;
    }
    (void)is_dir;
    return 1;
}

static int ensure_obuf(find_cfg *c, size_t need) {
    if (need > c->ocap) {
        size_t cap = c->ocap ? c->ocap : 256;
        while (cap < need) cap *= 2;
        char *nb = realloc(c->obuf, cap);
        if (!nb) return -1;
        c->obuf = nb; c->ocap = cap;
    }
    return 0;
}

static int find_parse(int argc, char **argv, int i, void **cfg_out) {
    find_cfg *c = calloc(1, sizeof *c);
    c->start = fp_xstrdup(".");
    c->maxdepth = -1;

    int j = i + 1;
    if (j < argc && argv[j][0] != '-') { free(c->start); c->start = fp_xstrdup(argv[j++]); }
    while (j < argc) {
        char *a = argv[j];
        if (strcmp(a, "-type") == 0 && j+1 < argc) {
            char t = argv[++j][0];
            if (t=='f' || t=='d') c->type = t;
            j++; continue;
        }
        if (strcmp(a, "-name") == 0 && j+1 < argc) {
            c->namepat = fp_xstrdup(argv[++j]); j++; continue;
        }
        if (strcmp(a, "-maxdepth") == 0 && j+1 < argc) {
            long md=0; if (fp_parse_long(argv[++j], &md)==0 && md>=0) c->maxdepth=(int)md; j++; continue;
        }
        if (strcmp(a, "-print0") == 0) { c->print0 = 1; j++; continue; }
        break; // stop for fx to read the next op token
    }
    *cfg_out = c;
    return j;
}

static int find_init_node(find_cfg *c) {
    // lstat to decide whether start is file or dir
    struct stat st;
    if (lstat(c->start, &st) != 0) { c->done = 1; return -1; }

    // Emit the start node itself if it matches and depth constraints allow.
    // find(1) typically considers depth 0 for the starting path.
    if (S_ISDIR(st.st_mode)) {
        // push directory frame for iteration
        if (c->maxdepth == 0) {
            // only the start path is considered; still may emit it if -type d etc.
            if (match_filters(c, c->start, &st, 1)) {
                size_t n = strlen(c->start);
                size_t need = n + 1; // suffix
                if (ensure_obuf(c, need+1) < 0) return -1;
                memcpy(c->obuf, c->start, n);
                c->obuf[n] = c->print0 ? '\0' : '\n';
                c->started = 1; c->done = 1;
                return 1; // produced
            }
            c->done = 1;
            return 0;
        }
        // If the dir itself matches filters, we should emit it before traversing (pre-order).
        if (match_filters(c, c->start, &st, 1)) {
            size_t n = strlen(c->start);
            size_t need = n + 1;
            if (ensure_obuf(c, need+1) < 0) return -1;
            memcpy(c->obuf, c->start, n);
            c->obuf[n] = c->print0 ? '\0' : '\n';
            // but still traverse it; mark started and fall through to push
            c->started = 1;
            // push for traversal
            if (push(c, c->start, 0, NULL) < 0) { c->done=1; return -1; }
            return 1; // produced the dir path first
        } else {
            // doesn't match type/name, but still traverse it
            if (push(c, c->start, 0, NULL) < 0) { c->done=1; return -1; }
            c->started = 1;
            return 0; // not produced yet
        }
    } else {
        // start is a file (or symlink, device — we’ll treat as file unless user set -type)
        if (match_filters(c, c->start, &st, 0)) {
            size_t n = strlen(c->start);
            size_t need = n + 1;
            if (ensure_obuf(c, need+1) < 0) return -1;
            memcpy(c->obuf, c->start, n);
            c->obuf[n] = c->print0 ? '\0' : '\n';
            c->started = 1; c->done = 1;
            return 1;
        }
        c->started = 1; c->done = 1;
        return 0;
    }
}

static int find_produce(void *vcfg, char **linep, size_t *lenp) {
    find_cfg *c = vcfg;
    if (c->done) return 0;

    // First call: handle start node
    if (!c->started) {
        int r = find_init_node(c);
        if (r != 0) {
            if (r < 0) return -1;
            if (r > 0) { *linep = c->obuf; *lenp = (c->print0 ? strlen(c->obuf) : (strlen(c->obuf))); // len recomputed below anyway
                size_t L = strlen(c->obuf) + 1; // includes suffix
                *lenp = L;
                return 1;
            }
        }
    }

    // DFS using our own stack
    while (c->top > 0) {
        Frame *fr = &c->stk[c->top - 1];

        // Open directory if not opened yet
        if (!fr->dir) {
            fr->dir = opendir(fr->path);
            if (!fr->dir) {
                pop(c); // unreadable; just skip
                continue;
            }
        }

        struct dirent *de = readdir(fr->dir);
        if (!de) {
            // directory exhausted
            pop(c);
            continue;
        }
        // skip . and ..
        if (de->d_name[0]=='.' && (de->d_name[1]=='\0' || (de->d_name[1]=='.' && de->d_name[2]=='\0')))
            continue;

        // Build child path
        size_t pn = strlen(fr->path);
        size_t dn = strlen(de->d_name);
        int need_slash = (pn && fr->path[pn-1] != '/');
        size_t clen = pn + (need_slash?1:0) + dn;
        char *child = malloc(clen + 1);
        if (!child) { c->done=1; return -1; }
        memcpy(child, fr->path, pn);
        if (need_slash) child[pn] = '/', pn++;
        memcpy(child + pn, de->d_name, dn);
        child[clen] = '\0';

        // lstat it
        struct stat st;
        if (lstat(child, &st) != 0) { free(child); continue; }

        int is_dir = S_ISDIR(st.st_mode);
        int depth_next = fr->depth + 1;

        // Emit child if it matches filters (find is pre-order)
        int emit_ok = match_filters(c, child, &st, is_dir);
        if (emit_ok) {
            size_t need = clen + 1; // suffix
            if (ensure_obuf(c, need+1) < 0) { free(child); c->done=1; return -1; }
            memcpy(c->obuf, child, clen);
            c->obuf[clen] = c->print0 ? '\0' : '\n';
            // If directory and we still may descend, push it AFTER emitting
            int can_descend = is_dir && (c->maxdepth < 0 || depth_next <= c->maxdepth);
            if (can_descend) { if (push(c, child, depth_next, NULL) < 0) { free(child); c->done=1; return -1; } }
            free(child);
            *linep = c->obuf;
            *lenp = c->print0 ? (clen + 1) : (clen + 1); // include suffix
            return 1;
        }

        // No emit; still descend if directory and within depth
        if (is_dir && (c->maxdepth < 0 || depth_next <= c->maxdepth)) {
            if (push(c, child, depth_next, NULL) < 0) { free(child); c->done=1; return -1; }
        }
        free(child);
    }

    c->done = 1;
    return 0;
}

static void find_destroy(void *vcfg) {
    find_cfg *c = vcfg;
    if (!c) return;
    for (; c->top > 0; ) pop(c);
    free(c->start);
    free(c->namepat);
    free(c->stk);
    free(c->obuf);
    free(c);
}

static const OpSpec SPEC = {
    .name="fp_find", .kind=OP_SRC,
    .parse=find_parse, .init=NULL,
    .consume=NULL, .produce=find_produce, .accept=NULL,
    .flush=NULL, .destroy=find_destroy, .should_stop=NULL
};
const OpSpec *op_find_spec(){ return &SPEC; }
