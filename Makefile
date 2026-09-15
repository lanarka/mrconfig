CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation
ALL_SRC = src/utils.c src/loader.c src/compiler.c

all: mrcfg

mrcfg: $(ALL_SRC) src/compiler.h src/loader.h src/mrcfg.h src/utils.h
	$(CC) $(CFLAGS) -o $@ $(ALL_SRC) src/main.c

clean:
	rm -f mrcfg dist/*.bin

.PHONY: all clean
