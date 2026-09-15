#include <stdio.h>
#include <string.h>
#include "loader.h"

void test_api();

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    test_api();
    return 0;
}

void test_api() {
    Config cfg = config_load("res.bin");
    config_print(&cfg);
    printf("------------------------------------------\n");

    config_set_int(&cfg,"global.meta.version", 66);
    config_update(&cfg);

    config_print(&cfg);
    config_free(&cfg);
}
