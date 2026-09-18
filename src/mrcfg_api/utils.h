/*
 * utils.h - small string/lookup helpers shared by the parser and the
 * binary loader. Nothing here is public API; it's included by loader.c
 * and compiler.c only.
 */

#ifndef UTILS_H
#define UTILS_H

#include "mrcfg.h"

#define MRCFG_MAGIC   "MRCFG"
#define MRCFG_VERSION 0x01

char     *strdup_s(const char *s);
char     *trim(char *s);
int       is_esc(const char *start, const char *p);
char     *find_outside_q(const char *s, char ch);
int       is_num(const char *t);
Value     parse_num(const char *s);
Section  *find_sec(Config *cfg, const char *name);
KeyValue *find_kv(Section *s, const char *name);
MapEntry *find_me(Value *mv, const char *key);
Value     deep_copy_val(const Value *v);

#endif
