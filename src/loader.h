#ifndef LOADER_H
#define LOADER_H

#include <stdio.h>
#include "mrcfg.h"

Config      config_load(const char *filename);
void        config_free(Config *cfg);
void        config_print(const Config *cfg);
const char *config_get_string(Config *cfg, const char *path);
long long   config_get_int(Config *cfg, const char *path);
double      config_get_float(Config *cfg, const char *path);
Value      *config_get_array(Config *cfg, const char *path, int *count);
Value      *config_get_map_entry(Config *cfg, const char *path);
const char *config_get_map_string(Config *cfg, const char *path);
long long   config_get_map_int(Config *cfg, const char *path);
double      config_get_map_float(Config *cfg, const char *path);
Value      *resolve_path(Config *cfg, Section *rel, const char *path);
void        free_val(Value *v);
int         config_set_int(Config *cfg, const char *path, long long value);
int         config_set_float(Config *cfg, const char *path, double value);
int         config_set_str(Config *cfg, const char *path, const char *value);
int         config_set_array(Config *cfg, const char *path, Value *items, int count);
int         config_set_map(Config *cfg, const char *path, MapEntry *entries, int count);
void        config_update(Config *cfg);

#endif
