#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make test
make stress ROWS="${ROWS:-100000}"
