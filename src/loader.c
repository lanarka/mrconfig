#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "loader.h"
#include "utils.h"

Value *resolve_path(Config *cfg, Section *rel, const char *path) {
    if (!path || !*path) return NULL;
    char tmp[512]; strncpy(tmp, path, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';

    char *parts[16]; int np = 0;
    char *tok = strtok(tmp, ".");
    while (tok && np<16) { parts[np++] = tok; tok = strtok(NULL, "."); }
    if (!np) return NULL;

    Section *sec = find_sec(cfg, parts[0]);
    int pi = sec ? 1 : 0;
    if (!sec) sec = rel;
    if (!sec || pi >= np) return NULL;

    KeyValue *kv = find_kv(sec, parts[pi++]);
    if (!kv) return NULL;
    Value *cur = &kv->value;

    for (int i = pi; i < np && cur; ++i) {
        if (cur->type == VAL_MAP) {
            MapEntry *me = find_me(cur, parts[i]);
            if (!me) return NULL;
            cur = &me->value;
        } else if (cur->type == VAL_ARRAY) {
            char *ep; long idx = strtol(parts[i], &ep, 10);
            if (*ep || idx<0 || idx>=cur->arr.count) return NULL;
            cur = &cur->arr.items[idx];
        } else return NULL;
    }
    return cur;
}

static Value *deref(Config *cfg, Section *sec, Value *v, int depth) {
    if (!v || depth > 256) return NULL;
    if (v->type != VAL_REF) return v;
    return deref(cfg, sec, resolve_path(cfg, sec, v->sval), depth+1);
}

static KeyValue *api_kv(Config *cfg, const char *path, Section **os) {
    char tmp[512]; strncpy(tmp, path, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';
    char *p0 = strtok(tmp, "."), *p1 = strtok(NULL, ".");
    if (!p0 || !p1) return NULL;
    Section *s = find_sec(cfg, p0); if (!s) return NULL;
    if (os) *os = s;
    return find_kv(s, p1);
}

static Section *sec_of(Config *cfg, const char *path) {
    char tmp[512]; strncpy(tmp, path, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';
    char *d = strchr(tmp, '.'); if (!d) return NULL; *d = '\0';
    return find_sec(cfg, tmp);
}

const char *config_get_string(Config *cfg, const char *path) {
    Section *s = NULL; KeyValue *kv = api_kv(cfg, path, &s); if (!kv) return NULL;
    Value *rv = deref(cfg, s, &kv->value, 0);
    return (rv && rv->type==VAL_STRING) ? rv->sval : NULL;
}

long long config_get_int(Config *cfg, const char *path) {
    Section *s = NULL; KeyValue *kv = api_kv(cfg, path, &s);
    if (!kv) { fprintf(stderr,"config_get_int: not found: %s\n",path); return 0; }
    Value *rv = deref(cfg, s, &kv->value, 0); if (!rv) return 0;
    if (rv->type==VAL_INT)   return rv->ival;
    if (rv->type==VAL_FLOAT) return (long long)rv->fval;
    fprintf(stderr,"config_get_int: type error: %s\n",path); return 0;
}

double config_get_float(Config *cfg, const char *path) {
    Section *s = NULL; KeyValue *kv = api_kv(cfg, path, &s);
    if (!kv) { fprintf(stderr,"config_get_float: not found: %s\n",path); return 0.0; }
    Value *rv = deref(cfg, s, &kv->value, 0); if (!rv) return 0.0;
    if (rv->type==VAL_FLOAT) return rv->fval;
    if (rv->type==VAL_INT)   return (double)rv->ival;
    fprintf(stderr,"config_get_float: type error: %s\n",path); return 0.0;
}

Value *config_get_array(Config *cfg, const char *path, int *count) {
    Section *s = NULL; KeyValue *kv = api_kv(cfg, path, &s); if (!kv) return NULL;
    Value *rv = deref(cfg, s, &kv->value, 0);
    if (!rv || rv->type!=VAL_ARRAY) return NULL;
    *count = rv->arr.count; return rv->arr.items;
}

Value *config_get_map_entry(Config *cfg, const char *path) {
    return resolve_path(cfg, NULL, path);
}
const char *config_get_map_string(Config *cfg, const char *path) {
    Value *v = resolve_path(cfg, NULL, path); if (!v) return NULL;
    Value *rv = deref(cfg, sec_of(cfg, path), v, 0);
    return (rv && rv->type==VAL_STRING) ? rv->sval : NULL;
}
long long config_get_map_int(Config *cfg, const char *path) {
    Value *v = resolve_path(cfg, NULL, path); if (!v) return 0;
    Value *rv = deref(cfg, sec_of(cfg, path), v, 0); if (!rv) return 0;
    if (rv->type==VAL_INT)   return rv->ival;
    if (rv->type==VAL_FLOAT) return (long long)rv->fval;
    return 0;
}
double config_get_map_float(Config *cfg, const char *path) {
    Value *v = resolve_path(cfg, NULL, path); if (!v) return 0.0;
    Value *rv = deref(cfg, sec_of(cfg, path), v, 0); if (!rv) return 0.0;
    if (rv->type==VAL_FLOAT) return rv->fval;
    if (rv->type==VAL_INT)   return (double)rv->ival;
    return 0.0;
}

static void xread(FILE *f, void *buf, size_t n) {
    if (fread(buf, 1, n, f) != n) {
        fprintf(stderr, "mrcfg: unexpected end of binary file\n");
        exit(1);
    }
}

static uint8_t read_u8(FILE *f) {
    uint8_t b; xread(f, &b, 1); return b;
}

static uint32_t read_u32be(FILE *f) {
    uint8_t b[4]; xread(f, b, 4);
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
}

static int64_t read_i64be(FILE *f) {
    uint8_t b[8]; xread(f, b, 8);
    uint64_t u = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48)
               | ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32)
               | ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16)
               | ((uint64_t)b[6] <<  8) |  (uint64_t)b[7];
    int64_t v; memcpy(&v, &u, 8); return v;
}

static double read_f64be(FILE *f) {
    uint8_t b[8]; xread(f, b, 8);
    uint64_t u = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48)
               | ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32)
               | ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16)
               | ((uint64_t)b[6] <<  8) |  (uint64_t)b[7];
    double v; memcpy(&v, &u, 8); return v;
}

static char *read_str(FILE *f) {
    uint32_t len = read_u32be(f);
    char *s = malloc(len);
    if (!s) { perror("malloc"); exit(1); }
    xread(f, s, len);
    return s;
}

static Value load_val(FILE *f) {
    Value v; memset(&v, 0, sizeof(v));
    v.type = (ValueType)read_u8(f);
    switch (v.type) {
        case VAL_STRING:
        case VAL_REF:
            v.sval = read_str(f);
            break;
        case VAL_INT:
            v.ival = (long long)read_i64be(f);
            break;
        case VAL_FLOAT:
            v.fval = read_f64be(f);
            break;
        case VAL_ARRAY: {
            uint32_t c = read_u32be(f);
            v.arr.count = (int)c;
            v.arr.items = calloc(c, sizeof(Value));
            if (!v.arr.items) { perror("calloc"); exit(1); }
            for (uint32_t i = 0; i < c; ++i)
                v.arr.items[i] = load_val(f);
        } break;
        case VAL_MAP: {
            uint32_t c = read_u32be(f);
            v.map.count = (int)c;
            v.map.entries = calloc(c, sizeof(MapEntry));
            if (!v.map.entries) { perror("calloc"); exit(1); }
            for (uint32_t i = 0; i < c; ++i) {
                v.map.entries[i].key   = read_str(f);
                v.map.entries[i].value = load_val(f);
            }
        } break;
        default:
            fprintf(stderr, "mrcfg: unknown value type 0x%02x in binary file\n", v.type);
            exit(1);
    }
    return v;
}

Config config_load(const char *fn) {
    FILE *f = fopen(fn, "rb");
    if (!f) { perror(fn); exit(1); }

    char magic[5]; xread(f, magic, 5);
    if (memcmp(magic, MRCFG_MAGIC, 5) != 0) {
        fprintf(stderr, "mrcfg: '%s' is not a valid MrCFG binary file\n", fn);
        exit(1);
    }
    uint8_t ver = read_u8(f);
    if (ver != MRCFG_VERSION) {
        fprintf(stderr, "mrcfg: binary version 0x%02x not supported (expected 0x%02x)\n",
                ver, MRCFG_VERSION);
        exit(1);
    }

    Config cfg; memset(&cfg, 0, sizeof(cfg));
    uint32_t sc = read_u32be(f);
    cfg.sectionCount = (int)sc;
    cfg.sections = calloc(sc, sizeof(Section));
    if (!cfg.sections) { perror("calloc"); exit(1); }

    for (uint32_t i = 0; i < sc; ++i) {
        Section *s = &cfg.sections[i];
        s->name = read_str(f);
        uint32_t kc = read_u32be(f);
        s->keyCount = (int)kc;
        s->keys = calloc(kc, sizeof(KeyValue));
        if (!s->keys) { perror("calloc"); exit(1); }
        for (uint32_t j = 0; j < kc; ++j) {
            s->keys[j].key      = read_str(f);
            s->keys[j].line_no  = -1;
            s->keys[j].filename = NULL;
            s->keys[j].value    = load_val(f);
        }
    }
    fclose(f);
    cfg._filename = strdup_s(fn);
    return cfg;
}

void free_val(Value *v) {
    if (!v) return;
    switch (v->type) {
        case VAL_STRING: case VAL_REF: if (v->sval) free(v->sval); break;
        case VAL_ARRAY:
            if (v->arr.items) {
                for (int i=0; i<v->arr.count; ++i) free_val(&v->arr.items[i]);
                free(v->arr.items);
            } break;
        case VAL_MAP:
            if (v->map.entries) {
                for (int i=0; i<v->map.count; ++i) {
                    free(v->map.entries[i].key);
                    free_val(&v->map.entries[i].value);
                }
                free(v->map.entries);
            } break;
        default: break;
    }
    v->type = VAL_UNKNOWN;
}

void config_free(Config *cfg) {
    if (!cfg) return;
    if (cfg->_filename) { free(cfg->_filename); cfg->_filename = NULL; }
    for (int i=0; i<cfg->sectionCount; ++i) {
        Section *s = &cfg->sections[i]; free(s->name);
        for (int j=0; j<s->keyCount; ++j) {
            free(s->keys[j].key);
            if (s->keys[j].filename) free((char*)s->keys[j].filename);
            free_val(&s->keys[j].value);
        }
        free(s->keys);
    }
    free(cfg->sections); cfg->sections = NULL; cfg->sectionCount = 0;
}

static void prv(const Value *v, int ind) {
    if (!v) return;
    switch (v->type) {
        case VAL_STRING: printf("\"%s\"", v->sval?v->sval:""); break;
        case VAL_REF:    printf("REF(%s)", v->sval?v->sval:""); break;
        case VAL_INT:    printf("%lld", (long long)v->ival); break;
        case VAL_FLOAT:  printf("%.17g", v->fval); break;
        case VAL_ARRAY:
            printf("[");
            for (int i=0; i<v->arr.count; ++i) { 
                if(i) printf(" "); 
                prv(&v->arr.items[i], ind); 
            }
            printf("]"); break;
        case VAL_MAP:
            printf("{\n");
            for (int i=0; i<v->map.count; ++i) {
                for (int s=0; s<ind+4; ++s) putchar(' ');
                printf("%s: ", v->map.entries[i].key);
                prv(&v->map.entries[i].value, ind+4);
                printf("\n");
            }
            for (int s=0; s<ind; ++s) putchar(' ');
            printf("}"); break;
        default: printf("?"); break;
    }
}

static Value *resolve_for_set(Config *cfg, const char *path) {
    if (path && !strchr(path, '.')) {
        fprintf(stderr,
            "mrcfg: config_set_*(\"%s\"): path must be \"Section.key\" "
            "(missing section prefix)\n", path);
    }
    return resolve_path(cfg, NULL, path);
}

static void replace_val(Value *v, Value newval) {
    free_val(v);
    *v = newval;
}

int config_set_int(Config *cfg, const char *path, long long value) {
    Value *v = resolve_for_set(cfg, path);
    if (!v) return 0;
    Value newval; memset(&newval, 0, sizeof(newval));
    newval.type = VAL_INT; newval.ival = value;
    replace_val(v, newval);
    return 1;
}

int config_set_float(Config *cfg, const char *path, double value) {
    Value *v = resolve_for_set(cfg, path);
    if (!v) return 0;
    Value newval; memset(&newval, 0, sizeof(newval));
    newval.type = VAL_FLOAT; newval.fval = value;
    replace_val(v, newval);
    return 1;
}

int config_set_str(Config *cfg, const char *path, const char *value) {
    Value *v = resolve_for_set(cfg, path);
    if (!v) return 0;
    Value newval; memset(&newval, 0, sizeof(newval));
    newval.type = VAL_STRING; newval.sval = strdup_s(value);
    replace_val(v, newval);
    return 1;
}

int config_set_array(Config *cfg, const char *path, Value *items, int count) {
    Value *v = resolve_for_set(cfg, path);
    if (!v) return 0;
    Value newval; memset(&newval, 0, sizeof(newval));
    newval.type = VAL_ARRAY;
    newval.arr.count = count;
    newval.arr.items = calloc((size_t)count, sizeof(Value));
    if (!newval.arr.items) { perror("calloc"); exit(1); }
    for (int i = 0; i < count; ++i)
        newval.arr.items[i] = deep_copy_val(&items[i]);
    replace_val(v, newval);
    return 1;
}

int config_set_map(Config *cfg, const char *path, MapEntry *entries, int count) {
    Value *v = resolve_for_set(cfg, path);
    if (!v) return 0;
    Value newval; memset(&newval, 0, sizeof(newval));
    newval.type = VAL_MAP;
    newval.map.count = count;
    newval.map.entries = calloc((size_t)count, sizeof(MapEntry));
    if (!newval.map.entries) { perror("calloc"); exit(1); }
    for (int i = 0; i < count; ++i) {
        newval.map.entries[i].key   = strdup_s(entries[i].key);
        newval.map.entries[i].value = deep_copy_val(&entries[i].value);
    }
    replace_val(v, newval);
    return 1;
}

extern void write_u8   (FILE *f, uint8_t  v);
extern void write_u32be(FILE *f, uint32_t v);
extern void write_i64be(FILE *f, int64_t  v);
extern void write_f64be(FILE *f, double   v);
extern void write_str  (FILE *f, const char *s);
extern void dump_val   (FILE *f, const Value *v);

void config_update(Config *cfg) {
    if (!cfg->_filename) {
        fprintf(stderr, "mrcfg: config_update: no filename known\n");
        exit(1);
    }
    FILE *f = fopen(cfg->_filename, "wb");
    if (!f) { perror(cfg->_filename); exit(1); }

    fwrite(MRCFG_MAGIC, 1, 5, f);
    write_u8(f, MRCFG_VERSION);
    write_u32be(f, (uint32_t)cfg->sectionCount);

    for (int i = 0; i < cfg->sectionCount; ++i) {
        Section *s = &cfg->sections[i];
        write_str(f, s->name);
        write_u32be(f, (uint32_t)s->keyCount);
        for (int j = 0; j < s->keyCount; ++j) {
            write_str(f, s->keys[j].key);
            dump_val(f, &s->keys[j].value);
        }
    }
    fclose(f);
}

void config_print(const Config *cfg) {
    for (int i=0; i<cfg->sectionCount; ++i) {
        const Section *s = &cfg->sections[i];
        printf("[%s]\n", s->name?s->name:"");
        for (int k=0; k<s->keyCount; ++k) {
            printf("  %s (line %d): ", s->keys[k].key?s->keys[k].key:"", s->keys[k].line_no);
            prv(&s->keys[k].value, 2);
            printf("\n");
        }
    }
}
