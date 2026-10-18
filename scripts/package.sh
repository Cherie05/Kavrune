#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$root/scripts/build.sh"
mkdir -p "$root/dist"
package_dir=$(mktemp -d "$root/dist/.package.XXXXXX")
trap 'rm -rf -- "$package_dir"' EXIT
trap 'exit 1' HUP INT TERM
(cd "$root/build" && cpack -G DEB -B "$package_dir")
for package in "$package_dir"/kavrune_*.deb; do
    python3 "$root/scripts/check-package.py" "$package" --self-test
    mv -- "$package" "$root/dist/"
done
python3 "$root/scripts/sbom.py" "$root/dist/sbom.spdx.json"
(cd "$root/dist" && sha256sum kavrune_*.deb sbom.spdx.json > SHA256SUMS)
cat "$root/dist/SHA256SUMS"
