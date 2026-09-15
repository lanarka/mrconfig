#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
#include "compiler.h"
#include "loader.h"
#include "utils.h"

#define ERR(file, line, ...) \
    do { fprintf(stderr, "%s:%d: ", (file), (line)); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         exit(1); } while(0)

#define ERR_NL(file, ...) \
    do { fprintf(stderr, "%s: ", (file)); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         exit(1); } while(0)

#define SLUG_MAX 64
#define MAX_INCLUDE_DEPTH 32

static void validate_slug(const char *name, const char *what,
                           const char *filename, int line_no) {
    if (!name || !*name) {
        ERR(filename, line_no, "%s name cannot be empty", what);
    }
    size_t len = strlen(name);
    if (len > SLUG_MAX) {
        ERR(filename, line_no,
            "%s name '%s' too long (%zu chars, max %d)",
            what, name, len, SLUG_MAX);
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!isalnum(c) && c != '_') {
            ERR(filename, line_no,
                "%s name '%s': invalid character '%c' "
                "(only A-Z a-z 0-9 _ allowed)",
                what, name, (char)c);
        }
    }
}

typedef struct { char *buf; int *lmap; } PreBuf;

static char *read_all(const char *fn) {
    FILE *f = fopen(fn, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *b = malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t n = fread(b, 1, (size_t)sz, f);
    b[n] = '\0'; fclose(f); return b;
}

static PreBuf preprocess(const char *src, const char *filename) {
    size_t n = strlen(src);
    char *dst = malloc(n + 1);
    int  *lm  = malloc(sizeof(int) * (n + 1));
    if (!dst || !lm) { perror("malloc"); exit(1); }
    size_t i = 0, j = 0;
    int inq = 0, line = 1;
    const char *st = src;
    while (src[i]) {
        lm[j] = line;
        if (inq) {
            if (src[i] == '\n') {
                fprintf(stderr, "%s:%d: unclosed string literal (missing closing '\"')\n",
                        filename, line);
                free(dst); free(lm); exit(1);
            }
            if (src[i]=='"' && !is_esc(st, src+i)) inq = 0;
            dst[j++] = src[i++];
        } else {
            if (src[i] == '\n') line++;
            if (src[i] == '"') { inq = 1; dst[j++] = src[i++]; continue; }
            if (src[i]=='/' && src[i+1]=='/') {
                i += 2; while (src[i] && src[i]!='\n') i++; continue;
            }
            if (src[i]=='/' && src[i+1]=='*') {
                int comment_line = line;
                i += 2;
                while (src[i] && !(src[i]=='*' && src[i+1]=='/')) {
                    if (src[i]=='\n') line++; i++;
                }
                if (!src[i]) {
                    fprintf(stderr, "%s:%d: unclosed block comment (missing closing '*/')\n",
                            filename, comment_line);
                    free(dst); free(lm); exit(1);
                }
                i += 2; continue;
            }
            if (src[i]=='\\' && (src[i+1]=='\n' || (src[i+1]=='\r' && src[i+2]=='\n'))) {
                i++;
                if (src[i]=='\r') i++;
                if (src[i]=='\n') { i++; line++; }
                while (src[i]==' ' || src[i]=='\t') i++;
                if (j > 0 && !isspace((unsigned char)dst[j-1])) {
                    dst[j] = ' '; lm[j] = line; j++;
                }
                continue;
            }
            if (src[i]=='\\' && src[i+1] != '\0') {
                fprintf(stderr, "%s:%d: '\\' must be followed by a newline (got: '%c')\n",
                        filename, line, src[i+1]);
                free(dst); free(lm); exit(1);
            }
            if (src[i]=='*' && src[i+1]=='/') {
                i += 2; continue;
            }
            dst[j++] = src[i++];
        }
    }
    if (inq) {
        fprintf(stderr, "%s:%d: unclosed string literal (missing closing '\"')\n",
                filename, line);
        free(dst); free(lm); exit(1);
    }
    dst[j] = '\0'; lm[j] = line;
    return (PreBuf){ dst, lm };
}

static Value parse_map_body  (char **pp, const char *orig, const char *filename, int line_no);
static Value parse_array_body(char **pp, const char *orig, const char *filename, int line_no);

static Value resolve_env(char **pp, const char *filename, int line_no) {
    char *s = *pp;
    if (!isalpha((unsigned char)*s) && *s != '_')
        ERR(filename, line_no, "'$' must be followed by an identifier");
    char name[256]; int ni = 0;
    while (*s && (isalnum((unsigned char)*s) || *s == '_')) {
        if (ni < (int)sizeof(name)-1) name[ni++] = *s;
        s++;
    }
    name[ni] = '\0';
    *pp = s;
    const char *val = getenv(name);
    if (!val)
        ERR(filename, line_no, "environment variable '$%s' is not set", name);
    Value v; memset(&v, 0, sizeof(v));
    if (is_num(val)) v = parse_num(val);
    else { v.type = VAL_STRING; v.sval = strdup_s(val); }
    return v;
}

static Value parse_tok(char **pp, const char *orig, const char *filename, int line_no) {
    char *s = *pp;
    while (*s && isspace((unsigned char)*s)) s++;
    Value v; memset(&v, 0, sizeof(v));

    if (*s == '"') {
        char b[4096]; int bi = 0;
        while (*s == '"') {
            s++;
            while (*s) {
                if (*s=='"' && !is_esc(orig, s)) { s++; break; }
                if (*s=='\\' && s[1]) {
                    char e = s[1];
                    if      (e=='n') b[bi++] = '\n';
                    else if (e=='t') b[bi++] = '\t';
                    else if (e=='r') b[bi++] = '\r';
                    else              b[bi++] = e;
                    s += 2;
                } else if (*s=='$' && !is_esc(orig, s)) {
                    s++;
                    if (!isalpha((unsigned char)*s) && *s != '_') {
                        if (bi < (int)sizeof(b)-1) b[bi++] = '$';
                    } else {
                        char name[256]; int ni = 0;
                        while (*s && (isalnum((unsigned char)*s) || *s == '_')) {
                            if (ni < (int)sizeof(name)-1) name[ni++] = *s;
                            s++;
                        }
                        name[ni] = '\0';
                        const char *ev = getenv(name);
                        if (!ev) {
                            ERR(filename, line_no, "environment variable '$%s' is not set", name);
                            exit(1);
                        }
                        size_t el = strlen(ev);
                        if (bi + (int)el < (int)sizeof(b)-1) { memcpy(b+bi, ev, el); bi += (int)el; }
                    }
                } else {
                    if (bi < (int)sizeof(b)-1) b[bi++] = *s;
                    s++;
                }
            }
            while (*s && isspace((unsigned char)*s)) s++;
        }
        b[bi] = '\0'; v.type = VAL_STRING; v.sval = strdup_s(b);

    } else if (*s == '{') {
        s++;
        v = parse_map_body(&s, orig, filename, line_no);

    } else if (*s == '(') {
        s++;
        v = parse_array_body(&s, orig, filename, line_no);

    } else if (*s == '$') {
        s++;
        v = resolve_env(&s, filename, line_no);

    } else {
        char *st = s;
        while (*s && !isspace((unsigned char)*s) && *s!=')' && *s!='(' && *s!='{' && *s!='}' && *s!=',') s++;
        size_t L = (size_t)(s - st);
        char buf[256];
        if (L < sizeof(buf)) { memcpy(buf, st, L); buf[L] = '\0'; }
        else                  { memcpy(buf, st, 255); buf[255] = '\0'; }
        char *t = trim(buf);
        if (is_num(t)) v = parse_num(t);
        else           { v.type = VAL_REF; v.sval = strdup_s(t); }
    }
    *pp = s;
    return v;
}

static Value parse_array_body(char **pp, const char *orig, const char *filename, int line_no) {
    Value av; memset(&av, 0, sizeof(av)); av.type = VAL_ARRAY;
    char *s = *pp;
    while (*s) {
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s)
            ERR(filename, line_no, "unclosed '(' — array is never closed with ')'");
        if (*s == ')') { s++; break; }

        Value elem = parse_tok(&s, orig, filename, line_no);
        int c = av.arr.count;
        Value *tmp = realloc(av.arr.items, sizeof(Value)*(c+1));
        if (!tmp) { perror("realloc"); exit(1); }
        av.arr.items = tmp; av.arr.items[c] = elem; av.arr.count = c+1;

        while (*s && isspace((unsigned char)*s)) s++;
        if      (*s == ',') { s++; }
        else if (*s == ')') { s++; break; }
        else if (*s)
            ERR(filename, line_no,
                "missing ',' between array elements (got: %.10s)", s);
    }
    *pp = s;
    return av;
}

static Value parse_map_body(char **pp, const char *orig, const char *filename, int line_no) {
    Value mv; memset(&mv, 0, sizeof(mv)); mv.type = VAL_MAP;
    char *s = *pp;

    while (*s) {
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s)
            ERR(filename, line_no, "unclosed '{' — map is never closed");
        if (*s == '}') { s++; break; }
        char *colon = NULL;
        { int q = 0;
          for (char *p = s; *p && *p!='}'; ++p) {
              if (*p=='"' && !is_esc(orig,p)) { q=!q; continue; }
              if (!q && *p==':') { colon = p; break; }
          }
        }
        if (!colon)
            ERR(filename, line_no, "map entry missing ':' near: %.40s", s);

        char kbuf[256];
        size_t kl = (size_t)(colon - s);
        if (kl >= sizeof(kbuf)) kl = sizeof(kbuf)-1;
        memcpy(kbuf, s, kl); kbuf[kl] = '\0';
        char *ekey = strdup_s(trim(kbuf));
        validate_slug(ekey, "map key", filename, line_no);

        s = colon + 1;
        while (*s && isspace((unsigned char)*s)) s++;

        Value ev; memset(&ev, 0, sizeof(ev));
        while (*s && isspace((unsigned char)*s)) s++;

        if (*s == '}' || !*s) {
            ev.type = VAL_UNKNOWN;
        } else if (*s == '{') {
            s++;
            ev = parse_map_body(&s, orig, filename, line_no);
        } else if (*s == '(') {
            s++;
            ev = parse_array_body(&s, orig, filename, line_no);
        } else {
            ev = parse_tok(&s, orig, filename, line_no);
            while (*s && isspace((unsigned char)*s)) s++;
            if (*s == ',') {
                ERR(filename, line_no,
                    "array in map must use '(': e.g.  key: (1, 2, 3)  not  key: 1, 2, 3");
            }
            if (*s && *s != '}') {
                int is_next_key = 0;
                if (*s != '"' && *s != '{' && *s != '(' && *s != '$') {
                    char *p = s;
                    while (*p && !isspace((unsigned char)*p) && *p!='}' && *p!='{' && *p!='(' && *p!=',') p++;
                    if (p > s && *(p-1) == ':') {
                        char tb[256]; size_t tl = (size_t)(p-1-s);
                        if (tl < sizeof(tb)) { memcpy(tb,s,tl); tb[tl]='\0'; }
                        else { memcpy(tb,s,255); tb[255]='\0'; }
                        if (!is_num(trim(tb))) is_next_key = 1;
                    }
                    if (!is_next_key) {
                        char *after = p;
                        while (*after && isspace((unsigned char)*after)) after++;
                        if (*after == ':') {
                            char tb[256]; size_t tl = (size_t)(p-s);
                            if (tl < sizeof(tb)) { memcpy(tb,s,tl); tb[tl]='\0'; }
                            else { memcpy(tb,s,255); tb[255]='\0'; }
                            if (!is_num(trim(tb))) is_next_key = 1;
                        }
                    }
                }
                if (!is_next_key)
                    ERR(filename, line_no,
                        "array in map must use '(': did you mean (%.10s, ...)?", s);
            }
        }
        int mc = mv.map.count;
        MapEntry *mt = realloc(mv.map.entries, sizeof(MapEntry)*(mc+1));
        if (!mt) { perror("realloc"); exit(1); }
        mv.map.entries = mt;
        mv.map.entries[mc].key   = ekey;
        mv.map.entries[mc].value = ev;
        mv.map.count = mc + 1;
    }
    *pp = s;
    return mv;
}

static void parse_seg(char *seg, KeyValue *kv, const char *filename) {
    char *s = seg;
    while (*s && isspace((unsigned char)*s)) s++;

    if (*s == '}')
        ERR(filename, kv->line_no, "unexpected '}' without opening '{'");
    if (*s == ')')
        ERR(filename, kv->line_no, "unexpected ')' without opening '('");

    if (*s == '{') {
        s++;
        kv->value = parse_map_body(&s, seg, filename, kv->line_no);
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s)
            ERR(filename, kv->line_no, "unexpected tokens after map '}': %.20s", s);
        return;
    }

    if (*s == '(') {
        s++;
        kv->value = parse_array_body(&s, seg, filename, kv->line_no);
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s)
            ERR(filename, kv->line_no, "unexpected tokens after array ')': %.20s", s);
        return;
    }

    kv->value = parse_tok(&s, seg, filename, kv->line_no);
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s)
        ERR(filename, kv->line_no,
            "unexpected token — array needs '(' and map needs '{': %.10s", s);
}


typedef struct { const char *files[MAX_INCLUDE_DEPTH]; int depth; } IncStack;
static IncStack g_inc = { {NULL}, 0 };

static void inc_push(const char *fn) {
    for (int i=0; i<g_inc.depth; ++i)
        if (strcmp(g_inc.files[i], fn)==0) {
            ERR_NL(fn, "circular include detected");
        }
    if (g_inc.depth >= MAX_INCLUDE_DEPTH) {
        ERR_NL(g_inc.files[g_inc.depth-1], "!include nesting limit (%d) exceeded", MAX_INCLUDE_DEPTH);
    }
    g_inc.files[g_inc.depth++] = fn;
}

static void inc_pop(void) { 
    if (g_inc.depth>0) g_inc.depth--; 
}

static Value eval_use_ref(Config *use_cfg, Value *v) {
    if (v->type == VAL_REF) {
        Value *t = resolve_path(use_cfg, NULL, v->sval);
        if (t) return eval_use_ref(use_cfg, t);
        return deep_copy_val(v);
    }
    if (v->type == VAL_ARRAY) {
        Value c; memset(&c,0,sizeof(c)); c.type=VAL_ARRAY;
        c.arr.count=v->arr.count;
        c.arr.items=calloc((size_t)v->arr.count,sizeof(Value));
        if(!c.arr.items){perror("calloc");exit(1);}
        for(int i=0;i<v->arr.count;++i) c.arr.items[i]=eval_use_ref(use_cfg,&v->arr.items[i]);
        return c;
    }
    if (v->type == VAL_MAP) {
        Value c; memset(&c,0,sizeof(c)); c.type=VAL_MAP;
        c.map.count=v->map.count;
        c.map.entries=calloc((size_t)v->map.count,sizeof(MapEntry));
        if(!c.map.entries){perror("calloc");exit(1);}
        for(int i=0;i<v->map.count;++i){
            c.map.entries[i].key=strdup_s(v->map.entries[i].key);
            c.map.entries[i].value=eval_use_ref(use_cfg,&v->map.entries[i].value);
        }
        return c;
    }
    return deep_copy_val(v);
}

static void apply_use(Config *cfg, Config *use_cfg) {
    for (int si=0; si<cfg->sectionCount; ++si) {
        Section *sec=&cfg->sections[si];
        for (int ki=0; ki<sec->keyCount; ++ki) {
            KeyValue *kv=&sec->keys[ki];
            Value resolved=eval_use_ref(use_cfg,&kv->value);
            free_val(&kv->value);
            kv->value=resolved;
        }
    }
}

static void parse_config_into(Config *cfg, const char *filename,
                              const char *caller_file, int caller_line) {
    inc_push(filename);
    char *src=read_all(filename);
    if (!src) {
        if (caller_file)
            fprintf(stderr, "%s:%d: can't open file '%s': %s\n",
                    caller_file, caller_line, filename, strerror(errno));
        else
            fprintf(stderr, "mrcfg: can't open file '%s': %s\n",
                    filename, strerror(errno));
        exit(1);
    }
    PreBuf pb=preprocess(src, filename); free(src);
    char *buf=pb.buf; int *lm=pb.lmap;
    char dir[512]; strncpy(dir,filename,sizeof(dir)-1); dir[sizeof(dir)-1]='\0';
    char *sl=strrchr(dir,'/'); if(sl) *(sl+1)='\0'; else dir[0]='\0';
    char *use_files[64]; int use_lines[64]; int use_count=0;
    int sec_before = cfg->sectionCount;

    Section *cur=NULL;
    char *p=buf;

    while (*p) {
        char *nl=strchr(p,'\n');
        size_t llen=nl?(size_t)(nl-p):strlen(p);
        int lineNo=lm[p-buf];

        char linebuf[2048];
        if(llen>=sizeof(linebuf)) llen=sizeof(linebuf)-1;
        memcpy(linebuf,p,llen); linebuf[llen]='\0';
        p=nl?nl+1:p+llen;

        char *ln=trim(linebuf);
        if(!*ln) continue;

        if (ln[0]=='!') {
            char directive[16]={0}, inc_file[512]={0};
            if (sscanf(ln,"!%15s \"%511[^\"]\"",directive,inc_file)!=2) {
                ERR(filename, lineNo, "expected  !include \"file\"  or  !use \"file\"");
                free(buf); free(lm); exit(1);
            }
            int do_include=(strcmp(directive,"include")==0);
            int do_use    =(strcmp(directive,"use"    )==0);
            if (!do_include && !do_use) {
                ERR(filename, lineNo, "unknown directive '!%s'", directive);
                free(buf); free(lm); exit(1);
            }
            char *full_path = malloc(1024);
            if(!full_path){perror("malloc");exit(1);}
            if (dir[0] && inc_file[0]!='/')
                snprintf(full_path,1024,"%s%s",dir,inc_file);
            else
                strncpy(full_path,inc_file,1023);

            if (do_include) {
                parse_config_into(cfg, full_path, filename, lineNo);
                free(full_path);
                cur=NULL;
            } else {
                if (use_count < 64) { use_files[use_count]=full_path; use_lines[use_count]=lineNo; use_count++; }
                else { ERR_NL(filename, "too many !use directives (max 64)"); }
            }
            continue;
        }

        if (ln[0]=='[') {
            char *rb=strchr(ln,']');
            if (!rb) ERR(filename, lineNo, "missing ']' in section header");
            char *after=trim(rb+1);
            if (*after)
                ERR(filename, lineNo,
                    "unexpected token after ']': '%s' (nothing is allowed after the section name)", after);
            *rb='\0'; char *sn=trim(ln+1);
            validate_slug(sn, "section", filename, lineNo);
            Section *ex=find_sec(cfg,sn);
            if (ex) {
                cur=ex;
            } else {
                cfg->sectionCount++;
                Section *tmp=realloc(cfg->sections,sizeof(Section)*cfg->sectionCount);
                if(!tmp){perror("realloc");exit(1);}
                cfg->sections=tmp; cur=&cfg->sections[cfg->sectionCount-1];
                cur->name=strdup_s(sn); cur->keyCount=0; cur->keys=NULL;
                cur->from_include=0;
            }
            continue;
        }
        if (strcmp(ln,")")==0) {
            ERR(filename, lineNo, "stray ')' — no matching '('");
            free(buf); free(lm); exit(1);
        }

        char *colon=find_outside_q(ln,':');
        if(!colon) { free(buf); free(lm); ERR(filename, lineNo, "expected ':'"); }
        if(!cur)   { free(buf); free(lm); ERR(filename, lineNo, "key outside any section"); }

        *colon='\0';
        char *key =trim(ln);
        validate_slug(key, "key", filename, lineNo);
        char *vraw=trim(colon+1);

        for(int i=0;i<cur->keyCount;++i)
            if(strcmp(cur->keys[i].key,key)==0) {
                ERR(filename, lineNo, "duplicate key '%s'", key);
                free(buf);free(lm);exit(1);
            }

        char *full=vraw, *acc=NULL; size_t acc_len=0;
        int depth=0;
        for(char *cp=vraw;*cp;++cp){if(*cp=='{')depth++;else if(*cp=='}')depth--;}
        if(depth<0){
            ERR(filename, lineNo, "unexpected '}' — missing opening '{'");
            free(buf);free(lm);exit(1);
        }
        if(depth>0){
            acc_len=strlen(vraw); acc=strdup_s(vraw);
            while(*p && depth>0){
                char *nnl=strchr(p,'\n');
                size_t nlen=nnl?(size_t)(nnl-p):strlen(p);
                char nbuf[2048]; if(nlen>=sizeof(nbuf))nlen=sizeof(nbuf)-1;
                memcpy(nbuf,p,nlen); nbuf[nlen]='\0';
                p=nnl?nnl+1:p+nlen;
                char *nln=trim(nbuf); if(!*nln) continue;
                size_t add=strlen(nln);
                char *tmp2=realloc(acc,acc_len+1+add+1);
                if(!tmp2){perror("realloc");exit(1);}
                acc=tmp2; acc[acc_len]=' ';
                memcpy(acc+acc_len+1,nln,add+1); acc_len+=1+add;
                for(char *cp=nln;*cp;++cp){if(*cp=='{')depth++;else if(*cp=='}')depth--;}
            }
            if(depth>0){
                ERR(filename, lineNo, "unclosed '{' — map is never closed");
                free(acc);free(buf);free(lm);exit(1);
            }
            full=acc;
        }

        cur->keyCount++;
        KeyValue *tmpkv=realloc(cur->keys,sizeof(KeyValue)*cur->keyCount);
        if(!tmpkv){perror("realloc");exit(1);}
        cur->keys=tmpkv;
        KeyValue *kv=&cur->keys[cur->keyCount-1];
        kv->key=strdup_s(key); kv->line_no=lineNo; kv->filename=strdup_s(filename);
        memset(&kv->value,0,sizeof(kv->value));
        parse_seg(full, kv, filename);
        if(acc) free(acc);
    }

    free(buf); free(lm);

    for (int i=0; i<use_count; ++i) {
        Config use_cfg; memset(&use_cfg,0,sizeof(use_cfg));
        parse_config_into(&use_cfg, use_files[i], filename, use_lines[i]);
        for (int si=sec_before; si<cfg->sectionCount; ++si) {
            Section *sec=&cfg->sections[si];
            for (int ki=0; ki<sec->keyCount; ++ki) {
                KeyValue *kv=&sec->keys[ki];
                Value resolved=eval_use_ref(&use_cfg,&kv->value);
                free_val(&kv->value);
                kv->value=resolved;
            }
        }
        config_free(&use_cfg);
        free(use_files[i]);
    }

    inc_pop();
}

static Config parse_config(const char *filename) {
    Config cfg; memset(&cfg,0,sizeof(cfg));
    parse_config_into(&cfg, filename, NULL, 0);
    return cfg;
}

static void chk_refs(Config *cfg, Section *sec, Value *v, int ln,
                     const char *filename, const char *cur_path) {
    if (!v) return;
    if (v->type == VAL_REF) {
        if (cur_path) {
            if (strcmp(v->sval, cur_path) == 0)
                ERR(filename, ln,
                    "self-reference: '%s' cannot reference itself", v->sval);
            const char *dot = strchr(cur_path, '.');
            if (dot && strcmp(v->sval, dot + 1) == 0)
                ERR(filename, ln,
                    "self-reference: '%s' is a relative reference to itself "
                    "(same as '%s')", v->sval, cur_path);
        }
        Value *t = resolve_path(cfg, sec, v->sval);
        if (!t) ERR(filename, ln, "unresolved reference '%s'", v->sval);
    } else if (v->type == VAL_ARRAY) {
        for (int i=0; i<v->arr.count; ++i)
            chk_refs(cfg, sec, &v->arr.items[i], ln, filename, cur_path);
    } else if (v->type == VAL_MAP) {
        for (int i=0; i<v->map.count; ++i)
            chk_refs(cfg, sec, &v->map.entries[i].value, ln, filename, cur_path);
    }
}

static void config_check_refs(Config *cfg) {
    for (int s=0; s<cfg->sectionCount; ++s) {
        Section *sec = &cfg->sections[s];
        for (int k=0; k<sec->keyCount; ++k) {
            KeyValue *kv = &sec->keys[k];
            char cur_path[256];
            snprintf(cur_path, sizeof(cur_path), "%s.%s", sec->name, kv->key);
            chk_refs(cfg, sec, &kv->value, kv->line_no,
                     kv->filename ? kv->filename : "<unknown>",
                     cur_path);
        }
    }
}

Config config_open(const char *filename) {
    Config cfg = parse_config(filename);
    config_check_refs(&cfg);
    return cfg;
}

void write_u8(FILE *f, uint8_t v) {
    fwrite(&v, 1, 1, f);
}

void write_u32be(FILE *f, uint32_t v) {
    uint8_t b[4];
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >>  8);
    b[3] = (uint8_t)(v      );
    fwrite(b, 1, 4, f);
}

void write_i64be(FILE *f, int64_t v) {
    uint64_t u = (uint64_t)v;
    uint8_t b[8];
    b[0] = (uint8_t)(u >> 56);
    b[1] = (uint8_t)(u >> 48);
    b[2] = (uint8_t)(u >> 40);
    b[3] = (uint8_t)(u >> 32);
    b[4] = (uint8_t)(u >> 24);
    b[5] = (uint8_t)(u >> 16);
    b[6] = (uint8_t)(u >>  8);
    b[7] = (uint8_t)(u      );
    fwrite(b, 1, 8, f);
}

void write_f64be(FILE *f, double v) {
    uint64_t u;
    memcpy(&u, &v, 8);
    uint8_t b[8];
    b[0] = (uint8_t)(u >> 56);
    b[1] = (uint8_t)(u >> 48);
    b[2] = (uint8_t)(u >> 40);
    b[3] = (uint8_t)(u >> 32);
    b[4] = (uint8_t)(u >> 24);
    b[5] = (uint8_t)(u >> 16);
    b[6] = (uint8_t)(u >>  8);
    b[7] = (uint8_t)(u      );
    fwrite(b, 1, 8, f);
}

void write_str(FILE *f, const char *s) {
    uint32_t len = (uint32_t)strlen(s) + 1;
    write_u32be(f, len);
    fwrite(s, 1, len, f);
}

void dump_val(FILE *f, const Value *v) {
    write_u8(f, (uint8_t)v->type);
    switch (v->type) {
        case VAL_STRING:
        case VAL_REF:
            write_str(f, v->sval);
            break;
        case VAL_INT:
            write_i64be(f, (int64_t)v->ival);
            break;
        case VAL_FLOAT:
            write_f64be(f, v->fval);
            break;
        case VAL_ARRAY:
            write_u32be(f, (uint32_t)v->arr.count);
            for (int i = 0; i < v->arr.count; ++i)
                dump_val(f, &v->arr.items[i]);
            break;
        case VAL_MAP:
            write_u32be(f, (uint32_t)v->map.count);
            for (int i = 0; i < v->map.count; ++i) {
                write_str(f, v->map.entries[i].key);
                dump_val(f, &v->map.entries[i].value);
            }
            break;
        default: break;
    }
}


void config_dump(Config *cfg, const char *fn) {
    FILE *f = fopen(fn, "wb");
    if (!f) { perror(fn); exit(1); }

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
