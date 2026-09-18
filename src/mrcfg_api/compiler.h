/*
 * compiler.h - text (.mrc) parser and binary (.bin) writer.
 *
 * config_open() is the normal entry point for reading a human-edited
 * config file. config_dump() serializes a Config to the portable
 * binary format described in the README, which config_load() (see
 * loader.h) can read back on any platform.
 */

#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdint.h>
#include "mrcfg.h"

Config config_open(const char *filename);
void   config_dump(Config *cfg, const char *filename);

/* Big-endian binary primitives, shared with loader.c's reader. Exposed
 * here mainly so both sides of the format stay in sync. */
void   write_u8   (FILE *f, uint8_t  v);
void   write_u32be(FILE *f, uint32_t v);
void   write_i64be(FILE *f, int64_t  v);
void   write_f64be(FILE *f, double   v);
void   write_str  (FILE *f, const char *s);
void   dump_val   (FILE *f, const Value *v);

#endif
