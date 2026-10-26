#!/bin/sh
# SPDX-License-Identifier: MIT
# Manual release step only: use an existing, explicitly selected publisher key.
# This script never generates keys, uploads artifacts or runs during packaging.
set -eu
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 EXISTING_GPG_KEY_FINGERPRINT [DIST_DIRECTORY]" >&2
    exit 2
fi
key=$1
case "$key" in *[!0123456789abcdefABCDEF]*|'') echo "Use the full hexadecimal GPG fingerprint." >&2; exit 2;; esac
if [ "${#key}" -ne 40 ] && [ "${#key}" -ne 64 ]; then
    echo "Use the full 40- or 64-character GPG fingerprint." >&2
    exit 2
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
directory=${2:-"$root/dist"}
directory=$(CDPATH= cd -- "$directory" && pwd)
sums="$directory/SHA256SUMS"
if [ ! -f "$sums" ] || [ -L "$sums" ]; then
    echo "A regular SHA256SUMS file is required. Build and inspect the release first." >&2
    exit 1
fi
if [ -e "$sums.asc" ] || [ -L "$sums.asc" ]; then
    echo "SHA256SUMS.asc already exists; inspect it before preparing another signature." >&2
    exit 1
fi
gpg --batch --list-secret-keys "$key" >/dev/null
(cd "$directory" && sha256sum --check SHA256SUMS)
temporary=$(mktemp -d "$directory/.sign.XXXXXX")
trap 'rm -rf -- "$temporary"' EXIT
trap 'exit 1' HUP INT TERM
gpg --batch --local-user "$key!" --armor --detach-sign --output "$temporary/SHA256SUMS.asc" "$sums"
gpg --verify "$temporary/SHA256SUMS.asc" "$sums"
mv -- "$temporary/SHA256SUMS.asc" "$sums.asc"
echo "Created $sums.asc. Publish the public-key fingerprint through an independently trusted channel."
echo "A valid signature authenticates this selected key; trust in the publisher requires checking its fingerprint."
