CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wno-misleading-indentation -Isrc/mrcfg_api

API_SRC = src/mrcfg_api/utils.c src/mrcfg_api/loader.c src/mrcfg_api/compiler.c
API_HDR = src/mrcfg_api/mrcfg.h src/mrcfg_api/loader.h src/mrcfg_api/compiler.h src/mrcfg_api/utils.h
API_OBJ = $(API_SRC:.c=.o)

EXAMPLE_SRC = $(wildcard examples/*.c)
EXAMPLE_BIN = $(EXAMPLE_SRC:.c=)

.PHONY: all cli api examples test clean

# Default target: CLI tool + the reusable static library.
all: cli api

#CLI: parses/prints or compiles a .mrc file (mrcfg_cli)
cli: mrcfg_cli

mrcfg_cli: src/mrcfg_cli/main.c $(API_SRC) $(API_HDR)
	$(CC) $(CFLAGS) -o $@ src/mrcfg_cli/main.c $(API_SRC)

#API: reusable static library for other projects (mrcfg_api)
api: libmrcfg_api.a

libmrcfg_api.a: $(API_OBJ)
	ar rcs $@ $(API_OBJ)

src/mrcfg_api/%.o: src/mrcfg_api/%.c $(API_HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

examples: $(EXAMPLE_BIN)

examples/%: examples/%.c libmrcfg_api.a
	$(CC) $(CFLAGS) -o $@ $< -L. -lmrcfg_api

test: test_api mrcfg_cli
	./test_api
	./tests/run_cli_tests.sh

test_api: tests/test_api.c libmrcfg_api.a
	@mkdir -p tests/tmp
	$(CC) $(CFLAGS) -o $@ tests/test_api.c -L. -lmrcfg_api -lm

clean:
	rm -f mrcfg_cli test_api libmrcfg_api.a $(API_OBJ) $(EXAMPLE_BIN)
	rm -rf dist/ tests/tmp/
