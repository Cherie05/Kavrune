#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -f "$root/.deps/env.sh" ]; then . "$root/.deps/env.sh"; fi
"$root/scripts/build.sh"
mkdir -p "$root/dist"
if [ -e "$root/dist/SHA256SUMS.asc" ] || [ -L "$root/dist/SHA256SUMS.asc" ]; then
    echo "Archive the previous signed release before rebuilding dist; SHA256SUMS.asc already exists." >&2
    exit 1
fi
package_dir=$(mktemp -d "$root/dist/.package.XXXXXX")
trap 'rm -rf -- "$package_dir"' EXIT
trap 'exit 1' HUP INT TERM
(cd "$root/build" && cpack -G DEB -B "$package_dir")
set --
for package in "$package_dir"/kavrune_*.deb; do
    python3 "$root/scripts/check-package.py" "$package" --self-test --offline --report "$root/dist/offline-package-check.json"
    target="$root/dist/$(basename -- "$package")"
    mv -- "$package" "$target"
    set -- "$@" "$target"
done
python3 "$root/scripts/sbom.py" "$root/dist/sbom.spdx.json"
python3 "$root/scripts/release-manifest.py" --output "$root/dist/release-manifest.json" "$@" "$root/dist/sbom.spdx.json" "$root/dist/offline-package-check.json"
(
    cd "$root/dist"
    for artifact in "$@"; do sha256sum "$(basename -- "$artifact")"; done
    sha256sum sbom.spdx.json offline-package-check.json release-manifest.json
) > "$root/dist/SHA256SUMS"
cat "$root/dist/SHA256SUMS"
