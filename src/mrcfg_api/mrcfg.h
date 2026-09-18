/*
 * mrcfg.h - core data types of the MrConfig format.
 *
 * A parsed (or loaded) config is a Config: a list of Sections, each
 * holding a list of KeyValue pairs. A Value is a small tagged union -
 * check `type` before reading the matching union member.
 *
 * These structs are shared by both the text parser (compiler.c) and the
 * binary loader (loader.c), so a Config looks identical no matter which
 * one produced it.
 */

#ifndef MRCFG_H
#define MRCFG_H

typedef enum {
    VAL_UNKNOWN,  // not yet set / parse error placeholder
    VAL_STRING,
    VAL_INT,
    VAL_FLOAT,
    VAL_ARRAY,
    VAL_REF,      // unresolved "Section.key" style reference
    VAL_MAP
} ValueType;

typedef struct Value    Value;
typedef struct MapEntry MapEntry;
typedef struct KeyValue KeyValue;
typedef struct Section  Section;
typedef struct Config   Config;

struct Value {
    ValueType type;
    union {
        char *sval;
        long long ival;
        double fval;
        struct {
            Value *items;
            int count;
        } arr;
        struct {
            MapEntry *entries;
            int count;
        } map;
    };
};

struct MapEntry {
    char *key;
    Value value;
};

struct KeyValue {
    char *key;
    Value value;
    int line_no;
    const char *filename;
};

struct Section {
    char *name;
    KeyValue *keys;
    int keyCount;
    int from_include;
};

struct Config {
    Section *sections;
    int sectionCount;
    char *_filename;
};

#endif
