/*
 * 03_modify_and_save.c - change a value in memory, write it out as a
 * portable ".bin", then reload the binary and modify it again in place.
 *
 * Two ways to persist changes:
 *   - config_dump(&cfg, path)   writes ANY Config to a new file.
 *   - config_update(&cfg)       re-writes the file a Config was
 *                                config_load()'ed from (it has no
 *                                effect on a Config from config_open(),
 *                                which was never loaded from a .bin).
 */
#include <stdio.h>
#include "mrcfg.h"
#include "compiler.h"
#include "loader.h"

int main(void) {
    // Parse the text config, bump the worker count, and dump it to a
    // fresh binary file.
    Config cfg = config_open("examples/app.mrc");
    printf("before: server.workers = %lld\n",
           config_get_int(&cfg, "server.workers"));

    config_set_int(&cfg, "server.workers", 8);
    config_dump(&cfg, "examples/app.bin");
    config_free(&cfg);
    printf("saved:  server.workers = 8  -> examples/app.bin\n");

    // Load the binary back and change it again, this time using
    // config_update() to save in place.
    Config bin = config_load("examples/app.bin");
    printf("loaded: server.workers = %lld\n",
           config_get_int(&bin, "server.workers"));

    config_set_int(&bin, "server.workers", 16);
    config_update(&bin);
    printf("updated in place: server.workers = 16\n");

    config_free(&bin);
    return 0;
}
