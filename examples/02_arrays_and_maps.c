/*
 * 02_arrays_and_maps.c - reading an array and a nested map from
 * examples/app.mrc ("server.tags" and "database.pool").
 */
#include <stdio.h>
#include "mrcfg.h"
#include "compiler.h"
#include "loader.h"

int main(void) {
    Config cfg = config_open("examples/app.mrc");

    // array: server.tags = ("web", "api", "v2")
    int count = 0;
    Value *tags = config_get_array(&cfg, "server.tags", &count);
    printf("server.tags (%d items):\n", count);
    for (int i = 0; i < count; ++i) {
        if (tags[i].type == VAL_STRING)
            printf("  [%d] \"%s\"\n", i, tags[i].sval);
    }

    // map: database.pool = { min: 2  max: 20  timeout: 30 }
    long long min = config_get_map_int(&cfg, "database.pool.min");
    long long max = config_get_map_int(&cfg, "database.pool.max");
    long long timeout = config_get_map_int(&cfg, "database.pool.timeout");
    printf("database.pool: min=%lld max=%lld timeout=%lld\n", min, max, timeout);

    config_free(&cfg);
    return 0;
}
