#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
exec make MODE=release benchmark ROWS="${1:-100000}"
