/*
 * mrcfg_cli - thin command-line front-end over the mrcfg_api library.
 * All the real work (parsing, compiling, loading) lives in the API;
 * this file is just argument handling + wiring.
 */

#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "loader.h"

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n"
        "  %s -c <source.mrc>                  Parse source and print\n"
        "  %s -c <source.mrc> -o <output.bin>  Compile source to binary\n"
        "  %s -l <config.bin>                  Load binary compilation and print\n"
        "\n", prog, prog, prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *input  = NULL;
    const char *output = NULL;
    int mode = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0) {
            if (mode) {
                fprintf(stderr, "error: -c and -l are mutually exclusive\n");
                usage(argv[0]); return 1;
            }
            mode = 'c';
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                fprintf(stderr, "error: -c requires a source filename\n");
                usage(argv[0]); return 1;
            }
            input = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0) {
            if (mode) {
                fprintf(stderr, "error: -c and -l are mutually exclusive\n");
                usage(argv[0]); return 1;
            }
            mode = 'l';
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                fprintf(stderr, "error: -l requires a binary filename\n");
                usage(argv[0]); return 1;
            }
            input = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: -o requires an output filename\n");
                usage(argv[0]); return 1;
            }
            output = argv[++i];
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            usage(argv[0]); return 1;
        }
    }
    if (!mode) {
        fprintf(stderr, "error: specify -c or -l\n");
        usage(argv[0]); return 1;
    }
    if (mode == 'l') {
        if (output) {
            fprintf(stderr, "error: -o is not valid with -l\n");
            usage(argv[0]); return 1;
        }
        Config cfg = config_load(input);
        config_print(&cfg);
        config_free(&cfg);
    } else {
        Config cfg = config_open(input);
        if (output) {
            config_dump(&cfg, output);
            printf("compiled: %s -> %s\n", input, output);
        } else {
            config_print(&cfg);
        }
        config_free(&cfg);
    }
    return 0;
}
