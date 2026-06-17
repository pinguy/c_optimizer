#!/bin/sh
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR" && exec ./run_c_optimizer.sh "$@"
