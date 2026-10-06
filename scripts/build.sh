#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
exec make "${1:-release}"
