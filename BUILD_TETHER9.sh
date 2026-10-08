#!/bin/sh
# Build the TETHER/9 example with the C Optimizer host-GCC pipeline.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COPT_EXTRA_LDLIBS=${COPT_EXTRA_LDLIBS:-'-ldl -lm'}
OUT=${OUT:-"$HERE/examples/tether9"}
export COPT_EXTRA_LDLIBS OUT
exec sh "$HERE/build_asm_syscall.sh" "$HERE/examples/TETHER9.c"
