#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdint.h>
#include "mrcfg.h"

Config config_open(const char *filename);
void   config_dump(Config *cfg, const char *filename);
void   write_u8   (FILE *f, uint8_t  v);
void   write_u32be(FILE *f, uint32_t v);
void   write_i64be(FILE *f, int64_t  v);
void   write_f64be(FILE *f, double   v);
void   write_str  (FILE *f, const char *s);
void   dump_val   (FILE *f, const Value *v);

#endif
