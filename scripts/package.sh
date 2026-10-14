#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$root/scripts/build.sh"
mkdir -p "$root/dist"
(cd "$root/build" && cpack -G DEB -B "$root/dist")
python3 "$root/scripts/sbom.py" "$root/dist/sbom.spdx.json"
(cd "$root/dist" && sha256sum kavrune_*.deb sbom.spdx.json > SHA256SUMS)
cat "$root/dist/SHA256SUMS"
