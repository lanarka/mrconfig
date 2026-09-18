/*
 * test_api.c - unit tests for the mrcfg_api library.
 *
 * These exercise the library directly (not the CLI) against the files
 * in samples/, so they must be run from the project root:
 *
 *     make test_api && ./test_api
 *     make test          (builds + runs this AND tests/run_cli_tests.sh)
 *
 * Kept dependency-free (no external test framework) on purpose, to
 * match the rest of the project's "just C99" philosophy.
 */
#define _POSIX_C_SOURCE 200112L /* for setenv() under -std=c99 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mrcfg.h"
#include "compiler.h"
#include "loader.h"

static int g_pass = 0, g_fail = 0;

#define CHECKF(cond, fmt, ...) do { \
        if (cond) { g_pass++; } \
        else { g_fail++; fprintf(stderr, "  FAIL %s:%d: " fmt "\n", \
                                  __FILE__, __LINE__, __VA_ARGS__); } \
    } while (0)

#define CHECK_INT(got, want) \
    CHECKF((long long)(got) == (long long)(want), \
           "%s == %lld, expected %lld", #got, (long long)(got), (long long)(want))

#define CHECK_DBL(got, want) \
    CHECKF(fabs((double)(got) - (double)(want)) < 1e-9, \
           "%s == %g, expected %g", #got, (double)(got), (double)(want))

#define CHECK_STR(got, want) do { \
        const char *_g = (got); const char *_w = (want); \
        CHECKF(_g && _w && strcmp(_g, _w) == 0, \
               "%s == \"%s\", expected \"%s\"", #got, _g ? _g : "(null)", _w); \
    } while (0)

#define CHECK_NOT_NULL(got) CHECKF((got) != NULL, "%s: expected non-NULL", #got)


/* Scalars, arrays and nested maps, all from samples/test5.mrc. Also
 * exercises $ABC env-var substitution inside an array. */
static void test_scalars_arrays_maps(void) {
    setenv("ABC", "Wow!", 1);
    Config cfg = config_open("samples/test5.mrc");

    int count = 0;
    Value *arr = config_get_array(&cfg, "hello.arr", &count);
    CHECK_NOT_NULL(arr);
    CHECK_INT(count, 3);
    CHECK_INT(arr[0].ival, 4);
    CHECK_INT(arr[1].ival, 1);
    CHECK_INT(arr[2].ival, 2);

    CHECK_INT(config_get_map_int(&cfg, "hello.mymap.a"), 123);
    CHECK_INT(config_get_map_int(&cfg, "hello.mymap.b"), -456);
    CHECK_STR(config_get_map_string(&cfg, "hello.mymap.c"), "Hello World's");
    CHECK_DBL(config_get_map_float(&cfg, "hello.mymap.d"), 1.17);
    CHECK_DBL(config_get_map_float(&cfg, "hello.mymap.e"), -123.5678);
    CHECK_INT(config_get_map_int(&cfg, "hello.mymap.f"), 0xff);

    CHECK_INT(config_get_map_int(&cfg, "hello.mymap.h"), 4);

    Value *i = config_get_map_entry(&cfg, "hello.mymap.i");
    CHECK_NOT_NULL(i);
    if (i) {
        CHECK_INT(i->type, VAL_ARRAY);
        CHECK_INT(i->arr.count, 4);
        CHECK_INT(i->arr.items[1].ival, 0xf);
    }

    Value *k = config_get_map_entry(&cfg, "hello.mymap.k");
    CHECK_NOT_NULL(k);
    if (k && k->type == VAL_ARRAY && k->arr.count == 2) {
        CHECK_STR(k->arr.items[0].sval, "Wow!");
        CHECK_STR(k->arr.items[1].sval, "Wow!");
    }

    config_free(&cfg);
}

/* Arbitrary-depth paths through nested maps, including a reference
 * (f1) and a reference into a nested array-of-arrays (f2), both
 * defined several levels deep in samples/test7.mrc. */
static void test_deep_paths_and_refs(void) {
    Config cfg = config_open("samples/test7.mrc");

    CHECK_STR(config_get_map_string(&cfg, "bar.root.nested.fox"), "Fox");
    CHECK_STR(config_get_map_string(&cfg, "bar.root.nested.nested.f1"), "Fox");
    /* f2: bar.root.nested.Array.6.0 -> Array[6] is (16,16), index 0 -> 16 */
    CHECK_INT(config_get_map_int(&cfg, "bar.root.nested.nested.f2"), 16);

    config_free(&cfg);
}

/* !include merges sections (here: two files both contributing keys to
 * the same [languages] section) and !use only resolves references
 * without keeping the used file's sections (here: [themes] pulling
 * colors from res/colors.mrc via res/theme.mrc). Both are driven from
 * samples/test.mrc. */
static void test_include_and_use(void) {
    Config cfg = config_open("samples/test.mrc");

    CHECK_STR(config_get_map_string(&cfg, "languages.en.greeting"), "Hello");
    CHECK_STR(config_get_map_string(&cfg, "languages.sk.greeting"), "Ahoj");

    /* !use resolves eagerly, pulling the concrete color value from
     * res/colors.mrc (via res/theme.mrc's own !use) straight into the
     * included [options]/[baz] config, with no [colors]/[themes]
     * section of its own surviving in the result. */
    CHECK_INT(config_get_map_int(&cfg, "themes.dark.main_window"), 0xff0000);

    // a top-level reference: [options].baz.bar -> [baz].bar (123)
    CHECK_INT(config_get_map_int(&cfg, "options.baz.bar"), 123);

    config_free(&cfg);
}

/* config_dump()/config_load() must round-trip every value type
 * (scalars, strings, arrays, maps) byte-for-byte in meaning, even
 * though the binary form always stores refs pre-resolved as-is. */
static void test_binary_roundtrip(void) {
    Config cfg = config_open("samples/test8.mrc");
    config_dump(&cfg, "tests/tmp_roundtrip.bin");
    config_free(&cfg);

    Config bin = config_load("tests/tmp_roundtrip.bin");
    CHECK_INT(config_get_int(&bin, "sekcia1.key1"), 55);
    CHECK_DBL(config_get_float(&bin, "sekcia1.key2"), 55.1);
    CHECK_STR(config_get_string(&bin, "sekcia1.key4"), "Hello");
    CHECK_INT(config_get_map_int(&bin, "Maps.mymap.a"), 123);
    CHECK_STR(config_get_map_string(&bin, "Maps.mymap.c"), "Hello World");

    config_free(&bin);
    remove("tests/tmp_roundtrip.bin");
}

/* config_set_*() + config_update() must persist through a save/reload
 * cycle on a file that was itself loaded from disk (config_update()
 * only works on a config_load()'ed Config - see loader.h). */
static void test_set_and_update(void) {
    Config cfg = config_open("samples/test6.mrc");
    config_dump(&cfg, "tests/tmp_update.bin");
    config_free(&cfg);

    Config bin = config_load("tests/tmp_update.bin");
    CHECK_INT(config_set_int(&bin, "foo.key0", 999), 1);
    CHECK_INT(config_set_float(&bin, "foo.key1", 2.5), 1);
    CHECK_INT(config_set_str(&bin, "foo.key3", "changed"), 1);
    // setting through a path with no section prefix must fail cleanly
    CHECK_INT(config_set_int(&bin, "key0", 1), 0);
    config_update(&bin);
    config_free(&bin);

    Config again = config_load("tests/tmp_update.bin");
    CHECK_INT(config_get_int(&again, "foo.key0"), 999);
    CHECK_DBL(config_get_float(&again, "foo.key1"), 2.5);
    CHECK_STR(config_get_string(&again, "foo.key3"), "changed");
    config_free(&again);
    remove("tests/tmp_update.bin");
}

int main(void) {
    test_scalars_arrays_maps();
    test_deep_paths_and_refs();
    test_include_and_use();
    test_binary_roundtrip();
    test_set_and_update();

    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
