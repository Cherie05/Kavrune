#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ ! -x "$root/build/kavrune" ]; then "$root/scripts/build.sh"; fi
exec "$root/build/kavrune" "$@"
