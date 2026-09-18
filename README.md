# MrConfig

A simple yet powerful configuration file format for C99 projects.  
Inspired by INI,TOML, extended with references, arrays, maps, environment variables, and an include system.

The project has two parts:

- **`mrcfg_api`** (`src/mrcfg_api/`) - the library: parses `.mrc` text, reads/writes the portable `.bin` format, and exposes the C API below. Build it as `libmrcfg_api.a` and link it into your own project.
- **`mrcfg_cli`** (`src/mrcfg_cli/`) - a thin command-line tool built on top of the library, for parsing/printing or compiling a config from the shell.

```
mrconfig/
├── src/
│   ├── mrcfg_api/   # the library (mrcfg_cli and your own code both link against this)
│   └── mrcfg_cli/   # the CLI tool's main.c
├── examples/        # small demo programs against mrcfg_api
├── tests/           # unit tests (test_api.c) + CLI smoke tests (run_cli_tests.sh)
└── samples/         # example .mrc files, also used by the tests
```

---

## Quick Start

```ini
[server]
host:    "localhost"
port:    8080
debug:   1
tags:    ("web", "api", "v2")

[database]
host:    server.host   // reference - shares value from [server]
port:    5432
```

```c
#include "mrcfg.h"
#include "compiler.h"   // config_open, config_dump
#include "loader.h"     // config_get_*, config_set_*, config_load

int main(void) {
    Config cfg = config_open("app.mrc");

    const char *host = config_get_string(&cfg, "server.host"); // "localhost"
    long long   port = config_get_int   (&cfg, "server.port"); // 8080

    config_free(&cfg);
    return 0;
}
```

---

## Syntax

### Sections

A section begins with a name in square brackets. Each section name must be unique.  
All keys defined after a header belong to that section.

```ini
[my_section]
key: value
```

---

### Keys and Values

Syntax: `key: value`

- Key names may contain letters, digits, `_`
- The separator is `:` (whitespace around it is optional)
- Duplicate keys within the same section are a **syntax error**

```ini
[config]
name:   "John"
age:    30
ratio:  1.5
```

---

### Value Types

| Type     | Examples                      | Notes                              |
|----------|-------------------------------|------------------------------------|
| `INT`    | `42`, `-7`, `0xFF`, `0b1010`  | decimal, hexadecimal, binary       |
| `FLOAT`  | `3.14`, `-0.5`, `1e9`         | IEEE 754 double                    |
| `STRING` | `"Hello World"`               | quotes required                    |
| `ARRAY`  | `(1, 2, 3)`                   | comma-separated values             |
| `MAP`    | `{a: 1 b: 2}`                 | key-value pairs inside parentheses |
| `REF`    | `section.key`                 | reference to another value         |

Numeric types are detected automatically — `0xFF` = `255` (INT), `3.14` = FLOAT.

```ini
[numbers]
dec:   -42
hex:   0xFF        // 255
bin:   0b1010      // 10
flt:   3.14159
exp:   1.5e3       // 1500.0
```

---

### Arrays

Multiple space-separated values form an array. Types may be mixed.

```ini
[data]
ints:    (1, 2, 3, 4, 5)
floats:  (1.1, 2.2, 3.3)
mixed:   (10, "hello", 3.14, 0xFF)
ports:   (8080, 8443, 9000)
```

```c
int count = 0;
Value *arr = config_get_array(&cfg, "data.ints", &count);
// arr[0].ival == 1,  arr[4].ival == 5
```

---

### Maps

A map is a collection of key-value pairs enclosed in `{` `}`.  
The opening parenthesis must be on the same line as the key.  
The closing `}` is **mandatory** — a missing parenthesis is a syntax error.

```ini
[app]
server: { host:  "localhost"
          port:  8080
          debug: 1
          tags:  (10, 20, 30)
          ratio: 0.75
}
```

Maps may contain references, arrays, and nested maps:

```ini
[world]
data: { nums:   (10, 20, 30)
        first:  world.data.nums.0    // reference to the first array element
        nested: {x: 1 y: 2}
}
```

```c
long long   port  = config_get_map_int   (&cfg, "app.server.port");   // 8080
double      ratio = config_get_map_float (&cfg, "app.server.ratio");  // 0.75
const char *host  = config_get_map_string(&cfg, "app.server.host");   // "localhost"

// array inside a map
Value *tags = config_get_map_entry(&cfg, "app.server.tags");
// tags->type == VAL_ARRAY,  tags->arr.count == 3

// indexed access via path
long long first = config_get_map_int(&cfg, "world.data.first");       // 10
```

---

### References

A reference is an identifier of the form `Section.key` or `Section.map.field`.  
References are resolved lazily through the API — they are stored as REF in the binary.

```ini
[base]
timeout: 30
host:    "db.local"

[app]
db_timeout: base.timeout   // resolves to 30
db_host:    base.host      // resolves to "db.local"

[cache]
data: { nums:  (1, 2, 3)
        first: cache.data.nums.0   // ref to array element inside map → 1
        timeout: base.timeout
}
```

Paths may have arbitrary depth: `Section.mapkey.field.N`

---

### Environment Variables

An environment variable is written as `$NAME` — substitution happens at parse time.  
If the variable is not set, parsing exits with an error message.

```ini
[server]
port:  $PORT             // e.g. PORT=8080  → int 8080
host:  $HOST             // e.g. HOST=localhost → string "localhost"
label: "server: $HOST"   // interpolation inside a string
```

```bash
PORT=8080 HOST=localhost ./mrcfg_cli -c example.mrc
```

The ENV value is automatically converted to INT or FLOAT when it looks like a number.

---

### Long Strings

Adjacent string literals are automatically concatenated (same as in C).  
This also works across multiple lines using `\` continuation.

```ini
[text]
msg:  "Hello " "World"                  // → "Hello World"

sql:  "SELECT * FROM users "  \
      "WHERE active = 1 "     \
      "ORDER BY name"

html: "<!DOCTYPE html>\n"  \
      "<html>\n"           \
      "  <body>text</body>\n" \
      "</html>\n"
```

---

### Comments

```ini
// single-line comment

/* multi-line
   comment */

[section]
eleven: 11   // inline comment
```

---

### Line Continuation

A backslash `\` at the end of a line joins the next line to the current one.

```ini
[data]
array: (1, 2, 3, \
        4, 5, 6, \
        7, 8, 9)

long_string: "part one " \
             "part two " \
             "part three"
```

---

### Directives !include and !use

#### `!include "file.mrc"`

Inserts the entire contents of the file — all its sections become part of the resulting config. References to included sections work normally (stored as REF).  
A section defined in an included file **cannot be redefined** — doing so is an error.

```ini
// defaults.mrc
[defaults]
timeout: 30
retries: 3
```

```ini
// app.mrc
!include "defaults.mrc"

[server]
timeout: defaults.timeout   // REF → 30
port:    8080
```

**Resulting config:**
```
[defaults]  
timeout: 30
retries: 3

[server]
timeout: REF(defaults.timeout)
port: 8080
```

#### `!use "file.mrc"`

Loads the file for reference resolution only — its sections are **not** added to the result. Any references pointing into the used file are immediately evaluated to concrete values.

```ini
// constants.mrc
[const]
pi: 3.14159
max_con: 100
```

```ini
// app.mrc
!use "constants.mrc"

[math]
circle_ratio: const.pi        // evaluated → 3.14159  (not a REF)
limit:        const.max_con   // evaluated → 100
```

**Resulting config:**
```
[math]
circle_ratio: 3.14159
limit: 100
```
*(section `[const]` is not present)*

#### Comparison

|                                   | `!include` | `!use`   |
|-----------------------------------|------------|----------|
| Sections merged into result       | ✓ yes      | ✗ no     |
| References                        | kept as REF | evaluated to values |
| Redefining an imported section    | **error**  | allowed  |
| Typical use                       | shared sections | constants, defaults |

Paths are resolved relative to the file containing the directive.  
Circular includes are detected and reported as an error.

---

## Complete Config Example

```ini
/*
 * Application configuration — app.mrc
 */

!use "constants.mrc"     // load constants without merging sections
!include "logging.mrc"   // merge [logging] section from another file

[app]
name:    "MyApp"
version: "1.0.0"
debug:   $DEBUG          // from environment, e.g. DEBUG=1

[server]
host:    $HOST
port:    $PORT
workers: 4
timeout: 30

// long string across multiple lines
banner:  "Welcome to MyApp\n"  \
         "Version 1.0.0\n"     \
         "All rights reserved\n"

[database]
host:    server.host     // shares host value from [server]
port:    5432
name:    "appdb"
pool: { min:     2
        max:     20
        timeout: 30
}

[cache]
ttls: (300, 150, 50)
keys: { users:   "user:*"
        sessions: "sess:*"
        default:  "app:*"
}
```

---

## C API

```c
#include "mrcfg.h"
#include "compiler.h"   // config_open, config_dump
#include "loader.h"     // everything else below

// Loading
Config cfg = config_open("app.mrc");   // parse text config
Config bin = config_load("app.bin");   // load binary file

// Saving
config_dump(&cfg, "app.bin");          // write portable binary
config_free(&cfg);                     // release all memory

// Scalar getters  (path = "Section.key")
const char *s = config_get_string(&cfg, "app.name");
long long   i = config_get_int   (&cfg, "server.port");
double      f = config_get_float (&cfg, "server.ratio");

// Array
int count = 0;
Value *arr = config_get_array(&cfg, "server.tags", &count);
// arr[0].type == VAL_STRING,  arr[0].sval == "..."

// Map
long long   max_val = config_get_map_int   (&cfg, "database.pool.max");
const char *path = config_get_map_string(&cfg, "logging.file.path");
double      flt  = config_get_map_float (&cfg, "database.pool.ratio");

// raw Value* for any type
Value *v = config_get_map_entry(&cfg, "database.pool.min");
if (v && v->type == VAL_INT) printf("%lld\n", v->ival);

// change value (by API)
config_set_int(&cfg,"server.timeout", 300);
config_update(&cfg);

// Debug print
config_print(&cfg);
```

### Function Reference

| Function | Description |
|----------|-------------|
| `config_open(file)` | Parse text config, check all references |
| `config_load(file)` | Load a binary config file |
| `config_dump(cfg, file)` | Write a portable binary snapshot |
| `config_free(cfg)` | Release all memory |
| `config_print(cfg)` | Debug-print all sections/keys to stdout |
| `config_get_string(cfg, path)` | Returns `const char*` or `NULL` |
| `config_get_int(cfg, path)` | Returns `long long` (0 if not found) |
| `config_get_float(cfg, path)` | Returns `double` (0.0 if not found) |
| `config_get_array(cfg, path, &count)` | Returns `Value*` and element count |
| `config_get_map_entry(cfg, path)` | Raw `Value*` for a map field |
| `config_get_map_string(cfg, path)` | String from a map field |
| `config_get_map_int(cfg, path)` | Integer from a map field |
| `config_get_map_float(cfg, path)` | Float from a map field |
| `config_set_int(cfg, path, value)` | Change integer value |
| `config_set_float(cfg, path, value)` | Change float value |
| `config_set_str(cfg, path, value)` | Change string value |
| `config_set_array(cfg, path, items, count)` | Change array list |
| `config_set_map(cfg, path, entries, count)` | Change map entries |
| `config_update(cfg)` | Update changes |



---

## CLI Tool

```bash
# Parse and print a text config
./mrcfg_cli -c config.mrc

# Compile a text config to a portable binary
./mrcfg_cli -c config.mrc -o output.bin

# Load a binary and print it
./mrcfg_cli -l output.bin
```

The binary file is identical across x86, ARM, MIPS, RISC-V — endianness and type sizes do not matter.

---

## Binary Format

The file begins with a 6-byte header: `MRCFG` (5 ASCII bytes) + version byte (`0x01`).  
All multi-byte numbers are written in **big-endian** (network byte order):

| Data type   | Stored as                              | Size     |
|-------------|----------------------------------------|----------|
| counts, lengths | `uint32_t` big-endian              | 4 bytes  |
| `VAL_INT`   | `int64_t` big-endian (two's complement) | 8 bytes  |
| `VAL_FLOAT` | IEEE 754 bit pattern as `uint64_t` BE  | 8 bytes  |
| string      | `uint32_t` length + raw bytes incl. NUL | variable |

---

## Limits

| Limit | Value |
|-------|-------|
| Max `!include` nesting depth | 32 |
| Max `!use` directives per file | 64 |
| Max line length (after preprocessing) | 2 047 characters |
| Max string literal / concat buffer | 4 095 characters |
| Max components in a dotted path | 16 (`A.b.c.d…`) |
| Max ENV variable name length | 255 characters |
| Max REF chain depth (cycle guard) | 256 |
| INT range | `int64_t` — ±9 223 372 036 854 775 807 |
| FLOAT range | `double` IEEE 754 |

---

## Build

```bash
make            # builds mrcfg_cli (CLI) and libmrcfg_api.a (API)
make cli        # just the CLI tool
make api        # just the static library
make examples   # small demo programs, see examples/README.md
make test       # unit tests (tests/test_api.c) + CLI smoke tests (tests/run_cli_tests.sh)
make clean
```

Requires only a C99 compiler (`gcc`/`clang`) and `make` — no external dependencies.

### Linking into your own project

Either link the prebuilt static library:

```bash
make api
gcc -std=c99 -Isrc/mrcfg_api -o my_app my_app.c -L. -lmrcfg_api
```

or compile the three library source files straight into your project:

```bash
gcc -std=c99 -Isrc/mrcfg_api -o my_app my_app.c \
    src/mrcfg_api/compiler.c src/mrcfg_api/loader.c src/mrcfg_api/utils.c
```

Either way, `#include "mrcfg.h"`, `"compiler.h"` and `"loader.h"` (from `src/mrcfg_api/`) is all you need — no other dependencies. See `examples/` for complete, runnable programs.

