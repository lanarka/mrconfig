#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"

/* strdup() is not part of C99, and we want a version that never returns
 * NULL silently, so every caller can skip the OOM check. */
char *strdup_s(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    if (!d) { perror("malloc"); exit(1); }
    memcpy(d, s, n + 1);
    return d;
}

char *trim(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) { *e = '\0'; --e; }
    return s;
}

/* Counts backslashes directly before p; an odd count means the char at
 * p is escaped (e.g. the closing '"' in "a\\" or the '$' in "\$FOO"). */
int is_esc(const char *start, const char *p) {
    int n = 0;
    const char *q = p - 1;
    while (q >= start && *q == '\\') { n++; q--; }
    return n % 2;
}

/* Like strchr(), but skips occurrences of ch that fall inside a
 * "quoted string". Used to find the ':' in a map entry without
 * matching one that happens to be inside a string value. */
char *find_outside_q(const char *s, char ch) {
    int q = 0;
    const char *o = s;
    for (const char *p = s; *p; ++p) {
        if (*p == '"' && !is_esc(o, p)) { q = !q; continue; }
        if (!q && *p == ch) return (char *)p;
    }
    return NULL;
}

/* Checks whether t is a full numeric literal: decimal/float, 0x hex, or
 * 0b binary, with an optional leading sign. Used to decide INT/FLOAT vs
 * STRING/REF for a bare (unquoted) token during parsing. */
int is_num(const char *t) {
    if (!t || !*t) return 0;
    const char *p = t;
    if (*p == '+' || *p == '-') p++;
    if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) {
        p += 2;
        if (!isxdigit((unsigned char)*p)) return 0;
        while (*p) { if (!isxdigit((unsigned char)*p)) return 0; p++; }
        return 1;
    }
    if (p[0]=='0' && (p[1]=='b'||p[1]=='B')) {
        p += 2;
        if (!*p) return 0;
        while (*p) { if (*p!='0' && *p!='1') return 0; p++; }
        return 1;
    }
    /* decimal/float must start with a digit here (sign already consumed) */
    if (!isdigit((unsigned char)*p)) return 0;
    int hd = 0, dot = 0, exp = 0;
    while (*p) {
        if (isdigit((unsigned char)*p)) hd = 1;
        else if (*p == '.' && !dot && !exp) dot = 1;
        else if ((*p=='e'||*p=='E') && hd && !exp) {
            exp = 1; p++;
            if (*p=='+' || *p=='-') p++;
            if (!isdigit((unsigned char)*p)) return 0;
            continue;
        } else return 0;
        p++;
    }
    return hd;
}

/* Converts a token already validated by is_num() into a Value. Floats
 * are detected by the presence of '.', 'e' or 'E'; everything else is
 * parsed as INT (decimal, 0x hex, or 0b binary). */
Value parse_num(const char *s) {
    Value v; memset(&v, 0, sizeof(v));
    if (strchr(s,'.') || strchr(s,'e') || strchr(s,'E')) {
        v.type = VAL_FLOAT; v.fval = strtod(s, NULL);
    } else {
        v.type = VAL_INT;
        if (s[0]=='0' && (s[1]=='x'||s[1]=='X'))
            v.ival = strtoll(s, NULL, 16);
        else if (s[0]=='0' && (s[1]=='b'||s[1]=='B')) {
            long long r = 0;
            for (const char *p = s+2; *p; ++p) r = r*2 + (*p=='1');
            v.ival = r;
        } else {
            v.ival = strtoll(s, NULL, 0);
        }
    }
    return v;
}

/* Linear lookups below: fine for typical config sizes (tens to a few
 * hundred sections/keys); would need an index if that ever changes. */

Section *find_sec(Config *cfg, const char *name) {
    for (int i=0; i<cfg->sectionCount; ++i)
        if (strcmp(cfg->sections[i].name, name)==0) return &cfg->sections[i];
    return NULL;
}

KeyValue *find_kv(Section *s, const char *name) {
    for (int i=0; i<s->keyCount; ++i)
        if (strcmp(s->keys[i].key, name)==0) return &s->keys[i];
    return NULL;
}

MapEntry *find_me(Value *mv, const char *key) {
    if (!mv || mv->type!=VAL_MAP) return NULL;
    for (int i=0; i<mv->map.count; ++i)
        if (strcmp(mv->map.entries[i].key, key)==0) return &mv->map.entries[i];
    return NULL;
}

/* Recursive deep copy, needed because config_set_array()/config_set_map()
 * take ownership of newly allocated storage rather than the caller's
 * original items (the caller may free or reuse them afterwards). */
Value deep_copy_val(const Value *v) {
    Value c; memset(&c, 0, sizeof(c)); c.type = v->type;
    switch (v->type) {
        case VAL_STRING:
        case VAL_REF:
            c.sval = strdup_s(v->sval);
            break;
        case VAL_INT:
            c.ival = v->ival;
            break;
        case VAL_FLOAT:
            c.fval = v->fval;
            break;
        case VAL_ARRAY:
            c.arr.count = v->arr.count;
            c.arr.items = calloc((size_t)v->arr.count, sizeof(Value));
            if (!c.arr.items) { perror("calloc"); exit(1); }
            for (int i = 0; i < v->arr.count; ++i)
                c.arr.items[i] = deep_copy_val(&v->arr.items[i]);
            break;
        case VAL_MAP:
            c.map.count = v->map.count;
            c.map.entries = calloc((size_t)v->map.count, sizeof(MapEntry));
            if (!c.map.entries) { perror("calloc"); exit(1); }
            for (int i = 0; i < v->map.count; ++i) {
                c.map.entries[i].key   = strdup_s(v->map.entries[i].key);
                c.map.entries[i].value = deep_copy_val(&v->map.entries[i].value);
            }
            break;
        default:
            break;
    }
    return c;
}
