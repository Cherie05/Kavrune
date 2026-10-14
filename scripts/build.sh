#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -f "$root/.deps/env.sh" ]; then . "$root/.deps/env.sh"; fi
cmake -S "$root" -B "$root/build" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON "$@"
cmake --build "$root/build" --parallel 2
ctest --test-dir "$root/build" --output-on-failure
