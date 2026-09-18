#!/usr/bin/env bash
# run_cli_tests.sh - black-box tests for mrcfg_cli, run from the project root:
#
#   make cli && ./tests/run_cli_tests.sh
#   (or just `make test`, which also runs the C unit tests first)
#
#   1) every sample in samples/ parses, compiles to .bin and loads back
#      without a non-zero exit code (a text -> binary -> text round trip);
#   2) every fixture in tests/fixtures/ is *correctly rejected* with a
#      non-zero exit code (each one is intentionally malformed).
#
# Set VALGRIND=1 to also run everything under valgrind --error-exitcode=1.

set -u
cd "$(dirname "$0")/.."

CLI=./mrcfg_cli
TMP=tests/tmp
PASS=0
FAIL=0

if [ ! -x "$CLI" ]; then
    echo "error: $CLI not built - run 'make cli' first" >&2
    exit 1
fi

RUNNER=""
if [ "${VALGRIND:-0}" = "1" ]; then
    RUNNER="valgrind --error-exitcode=1 --leak-check=full --quiet"
fi

ok()   { PASS=$((PASS+1)); echo "  ok   $1"; }
bad()  { FAIL=$((FAIL+1)); echo "  FAIL $1: $2"; }

mkdir -p "$TMP"

echo "== samples/*.mrc: parse, compile, reload =="
for src in samples/*.mrc; do
    name=$(basename "$src" .mrc)
    bin="$TMP/$name.bin"

    # samples/test3.mrc, test5.mrc, test8.mrc, test9.mrc use $ABC
    if ! ABC="Wow!" $RUNNER $CLI -c "$src" > /dev/null 2> "$TMP/$name.err"; then
        bad "$src" "parse (-c) failed: $(tail -1 "$TMP/$name.err")"
        continue
    fi
    if ! ABC="Wow!" $RUNNER $CLI -c "$src" -o "$bin" > /dev/null 2>> "$TMP/$name.err"; then
        bad "$src" "compile (-c -o) failed"
        continue
    fi
    if ! $RUNNER $CLI -l "$bin" > /dev/null 2>> "$TMP/$name.err"; then
        bad "$src" "reload (-l) of compiled binary failed"
        continue
    fi
    ok "$src"
done

echo "== tests/fixtures/*.mrc: must be rejected =="
for src in tests/fixtures/*.mrc; do
    if $CLI -c "$src" > /dev/null 2> "$TMP/fixture.err"; then
        bad "$src" "expected a non-zero exit code, got success"
    else
        ok "$src -> $(tail -1 "$TMP/fixture.err")"
    fi
done

rm -rf "$TMP"

echo
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
