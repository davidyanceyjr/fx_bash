#ifndef FP_UTIL_H
#define FP_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* Large stdio buffers */
#ifndef FP_BUF_1M
#define FP_BUF_1M (1<<20)
#endif

/* printf-like error helper */
void fp_errf(const char *who, int k, const char *name, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* ---- ASCII helpers ---- */
static inline int fp_ascii_tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}
static inline int fp_ascii_toupper(int c) {
    return (c >= 'a' && c <= 'z') ? (c - 32) : c;
}
/* Back-compat with your earlier names */
static inline int ascii_tolower(int c) { return fp_ascii_tolower(c); }
static inline int ascii_toupper(int c) { return fp_ascii_toupper(c); }

/* ASCII-only; supports ranges like a-z, classes [:lower:], [:upper:], and options -d (delete), -s (squeeze) */
typedef struct {
    unsigned char map[256];      /* mapping for bytes when selected[c] == 1 */
    unsigned char selected[256]; /* membership of SET1 */
    int delete_mode;             /* -d */
    int squeeze_mode;            /* -s */
} fp_trspec;


/* ---- Fieldset API (used by cut) ---- */
typedef struct {
    uint8_t *bits;     /* bitmask, 1 bit per field */
    size_t   nbits;    /* number of addressable bits */
    int      has_ranges;
} fp_fieldset;

/* Build transliteration specification from SET1 and SET2 (SET2 ignored when -d) */
int  fp_trspec_build(fp_trspec *t, const char *set1, const char *set2);

/* Apply transliteration in-place to a single record buffer */
void fp_tr_inplace(char **linep, size_t *lenp, const fp_trspec *t);

/* Parse a LIST like "1,3-5" into bitset. Returns 0 on success, <0 on error. */
int  fp_fieldset_parse(const char *list, fp_fieldset *fs);

/* Test if field number idx1 (1-based) is in set. Returns 1 yes, 0 no. */
int  fp_fieldset_has(fp_fieldset *fs, size_t idx1);

/* Free resources allocated in fp_fieldset_parse(). */
void fp_fieldset_free(fp_fieldset *fs);

/* ---- Your existing field list API (kept for compatibility) ---- */
typedef struct {
    int start; /* inclusive */
    int end;   /* inclusive */
} fp_field_range;

typedef struct {
    fp_field_range *ranges;
    size_t nranges;
} fp_field_list;

int  fp_parse_field_list(const char *s, fp_field_list *out);
void fp_free_field_list(fp_field_list *fl);

/* ---- Regex / fixed wrapper (your interface) ---- */
typedef struct {
    regex_t rx;
    int     is_fixed;
    int     icase;
    char   *fixed_pat;
} fp_regex;

int  fp_regex_compile(fp_regex *r, const char *pat, int extended, int icase, int fixed);
int  fp_regex_match(fp_regex *r, const char *s, size_t len);
void fp_regex_free(fp_regex *r);

/* ---- Grep spec shim used by op_grep.c (built atop fp_regex) ---- */
typedef struct {
    int use_regex;     /* 1 => regex; 0 => fixed */
    int ignore_case;   /* ASCII casefold */
    int invert;        /* -v */
    long max_matches;  /* -m N (<=0 unlimited) */
    long matched;      /* running count */

    /* Under the hood we reuse fp_regex for both fixed/regex */
    fp_regex rewrap;

    /* For fixed-matcher fast path we also keep the raw pattern/len */
    char  *fixed;
    size_t fixed_len;
} fp_grepspec;

int  fp_grepspec_compile(fp_grepspec *g, int extended, int fixed, int ignore_case, const char *pattern);
int  fp_grepspec_match_line(fp_grepspec *g, const char *s, size_t len); /* 1 match, 0 no */
void fp_grepspec_free(fp_grepspec *g);

/* ---- Misc ---- */
char *fp_xstrdup(const char *s);
int   fp_parse_long(const char *s, long *out);

#endif /* FP_UTIL_H */
