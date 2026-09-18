/*
 * loader.h - public read/write API for an already-built Config.
 *
 * This header is used both for loading compiled ".bin" files
 * (config_load) and for querying/mutating a Config that came from
 * config_open() in compiler.h. Path arguments are dotted strings such
 * as "server.port" or "app.pool.max" (map field) / "app.tags.0" (array
 * index).
 */
#ifndef LOADER_H
#define LOADER_H

#include <stdio.h>
#include "mrcfg.h"

Config      config_load(const char *filename);   /* load a compiled .bin file */
void        config_free(Config *cfg);
void        config_print(const Config *cfg);      /* debug dump to stdout */

/* Scalar getters at top-level "Section.key" paths. References (REF) are
 * followed automatically. On error/missing path: string returns NULL,
 * numbers return 0 / 0.0 and a message is printed to stderr. */
const char *config_get_string(Config *cfg, const char *path);
long long   config_get_int(Config *cfg, const char *path);
double      config_get_float(Config *cfg, const char *path);
Value      *config_get_array(Config *cfg, const char *path, int *count);

Value      *config_get_map_entry(Config *cfg, const char *path);
const char *config_get_map_string(Config *cfg, const char *path);
long long   config_get_map_int(Config *cfg, const char *path);
double      config_get_map_float(Config *cfg, const char *path);

/* Generic path resolver used internally by the getters above; exposed
 * because it is also handy for walking into arrays/maps directly. */
Value      *resolve_path(Config *cfg, Section *rel, const char *path);
void        free_val(Value *v);

/* In-place mutation of an existing key ("Section.key" path required).
 * Returns 1 on success, 0 if the path doesn't resolve. */
int         config_set_int(Config *cfg, const char *path, long long value);
int         config_set_float(Config *cfg, const char *path, double value);
int         config_set_str(Config *cfg, const char *path, const char *value);
int         config_set_array(Config *cfg, const char *path, Value *items, int count);
int         config_set_map(Config *cfg, const char *path, MapEntry *entries, int count);

/* Persist the current in-memory state back to the file config_load()
 * opened it from. Requires the Config to have a known filename. */
void        config_update(Config *cfg);

#endif
