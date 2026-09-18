/*
 * 01_read_values.c - the smallest possible use of mrcfg_api: open a
 * text config and read a few scalar values out of it.
 *
 * Build:  make examples        (or manually, see README "Linking")
 * Run:    ./examples/01_read_values
 */
#include <stdio.h>
#include "mrcfg.h"
#include "compiler.h"
#include "loader.h"

int main(void) {
    Config cfg = config_open("examples/app.mrc");

    const char *host = config_get_string(&cfg, "server.host");
    long long   port = config_get_int(&cfg, "server.port");
    long long   workers = config_get_int(&cfg, "server.workers");

    printf("server.host    = %s\n", host);
    printf("server.port    = %lld\n", port);
    printf("server.workers = %lld\n", workers);

    /* "database.host" is a reference to "server.host" in app.mrc -
     * config_get_string() follows it automatically. */
    printf("database.host  = %s (resolved via reference)\n",
           config_get_string(&cfg, "database.host"));

    config_free(&cfg);
    return 0;
}
