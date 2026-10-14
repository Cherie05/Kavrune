#!/bin/sh
set -eu
cli="$1"
python3 - "$cli" <<'PY'
import json
import subprocess
import sys
result = subprocess.run([sys.argv[1], 'self-test', '--json'], check=True, capture_output=True)
data = json.loads(result.stdout)
assert data['passed'] is True
assert data['physical_usb_tested'] is False
assert data['bytes'] == 8 * 1024 * 1024 + 37
assert len(data['sha256']) == 64
PY
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
python3 - "$scratch/source.img" <<'PY'
import sys
from pathlib import Path
Path(sys.argv[1]).write_bytes(bytes(range(256)) * 16385)
PY
"$cli" inspect "$scratch/source.img" --json
"$cli" simulate "$scratch/source.img" "$scratch/target.img"
cmp "$scratch/source.img" "$scratch/target.img"
expected=$(sha256sum "$scratch/source.img")
expected=${expected%% *}
"$cli" checksum "$scratch/target.img" --expected "$expected"
if "$cli" simulate "$scratch/source.img" "$scratch/target.img"; then
  echo 'FAIL: simulation overwrote an existing target' >&2
  exit 1
fi
if "$cli" checksum "$scratch/source.img" --expected 00; then
  echo 'FAIL: checksum mismatch accepted' >&2
  exit 1
fi
ln -s "$scratch/source.img" "$scratch/symlink.img"
if "$cli" inspect "$scratch/symlink.img"; then
  echo 'FAIL: symlink accepted' >&2
  exit 1
fi
python3 - "$cli" "$scratch" <<'PY'
import json
import os
import subprocess
import sys
filename = os.fsencode(sys.argv[2]) + b'/invalid-\xff.img'
with open(filename, 'wb') as file:
    file.write(b'raw test image')
result = subprocess.run([os.fsencode(sys.argv[1]), b'inspect', filename, b'--json'], check=True, capture_output=True)
data = json.loads(result.stdout)
assert data['size'] == 14
assert data['raw_writable'] is True
PY
