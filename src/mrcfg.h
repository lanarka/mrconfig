#ifndef MRCFG_H
#define MRCFG_H

typedef enum {
    VAL_UNKNOWN,
    VAL_STRING,
    VAL_INT,
    VAL_FLOAT,
    VAL_ARRAY,
    VAL_REF,
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
