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
python3 - "$cli" "$scratch" <<'PY'
import json
import struct
import subprocess
import sys
from pathlib import Path

cli, scratch = sys.argv[1], Path(sys.argv[2])

def both32(data, offset, value):
    struct.pack_into('<I', data, offset, value)
    struct.pack_into('>I', data, offset + 4, value)

def record(name, extent, length, directory):
    name = name.encode('ascii')
    data = bytearray(33 + len(name) + (len(name) % 2 == 0))
    data[0] = len(data)
    both32(data, 2, extent)
    both32(data, 10, length)
    data[25] = 2 if directory else 0
    data[28], data[31], data[32] = 1, 1, len(name)
    data[33:33 + len(name)] = name
    return data

def fixture(windows=False):
    data = bytearray(32 * 2048)
    # A full-image, type-zero active MBR carrier accompanies this bootable ISO.
    # Its EFI image is in a section; there is no bootable default entry.
    data[446] = 0x80
    struct.pack_into('<I', data, 458, len(data) // 512)
    data[510:512] = b'\x55\xaa'
    pvd = 16 * 2048
    data[pvd:pvd + 7] = b'\x01CD001\x01'
    both32(data, pvd + 80, 32)
    data[pvd + 128:pvd + 132] = b'\x00\x08\x08\x00'
    root = record('\x00', 20, 2048, True)
    data[pvd + 156:pvd + 156 + len(root)] = root
    data[20 * 2048:20 * 2048 + len(root)] = root
    if windows:
        entries = record('SOURCES', 21, 2048, True) + record('BOOTMGR;1', 23, 16, False)
        data[20 * 2048 + len(root):20 * 2048 + len(root) + len(entries)] = entries
        installer = record('INSTALL.WIM;1', 24, 16, False)
        data[21 * 2048:21 * 2048 + len(installer)] = installer
    boot = 17 * 2048
    data[boot:boot + 7] = b'\x00CD001\x01'
    data[boot + 7:boot + 30] = b'EL TORITO SPECIFICATION'
    struct.pack_into('<I', data, boot + 71, 22)
    data[18 * 2048:18 * 2048 + 7] = b'\xffCD001\x01'
    catalog = 22 * 2048
    data[catalog] = 1
    data[catalog + 30:catalog + 32] = b'\x55\xaa'
    struct.pack_into('<H', data, catalog + 28, (-1 - 0xaa55) & 0xffff)
    data[catalog + 64], data[catalog + 65] = 0x91, 0xef
    struct.pack_into('<H', data, catalog + 66, 1)
    data[catalog + 96] = 0x88
    struct.pack_into('<I', data, catalog + 104, 23)
    assert len(data) == 32 * 2048
    return data

def run(*args, success=True):
    result = subprocess.run([cli, *map(str, args)], capture_output=True)
    assert (result.returncode == 0) == success, (args, result.stderr.decode(errors='replace'))
    return result

def problem(path, issue):
    info = json.loads(run('inspect', path, '--json', success=False).stdout)
    assert info['issue'] == issue and info['error'] and info['advice'], info
    return info

source = scratch / 'uefi-section.iso'
source.write_bytes(fixture())
info = json.loads(run('inspect', source, '--json').stdout)
assert info['kind'] == 'Hybrid ISO' and info['raw_writable'] and info['bootable']
target = scratch / 'uefi-section-simulated.img'
run('simulate', source, target)
assert source.read_bytes() == target.read_bytes()
windows = scratch / 'uefi-windows.iso'
windows.write_bytes(fixture(windows=True))
info = json.loads(run('inspect', windows, '--json').stdout)
assert info['kind'] == 'Windows installation ISO' and not info['raw_writable']
blocked_output = scratch / 'blocked-output.img'
run('simulate', windows, blocked_output, success=False)
assert not blocked_output.exists()
for suffix in ('iso.crdownload', 'iso.part', 'iso.partial', 'iso.download'):
    partial = scratch / ('unfinished.' + suffix)
    partial.write_bytes(source.read_bytes())
    problem(partial, 'incomplete')
    run('simulate', partial, blocked_output, success=False)
    assert not blocked_output.exists()
truncated = scratch / 'truncated.raw'
truncated.write_bytes(source.read_bytes()[:-2048])
problem(truncated, 'malformed')
run('simulate', truncated, blocked_output, success=False)
assert not blocked_output.exists()
unknown = scratch / 'unknown.iso'
unknown.write_bytes(b'This is an ordinary file, not an ISO.')
unknown_problem = problem(unknown, 'unrecognized')
plain_error = run('inspect', unknown, success=False).stderr.decode()
assert unknown_problem['advice'] in plain_error
compressed = scratch / 'compressed.img.xz'
compressed.write_bytes(b'\xfd7zXZ\x00')
problem(compressed, 'compressed')
container = scratch / 'virtual.qcow2'
container.write_bytes(b'QFI\xfb')
problem(container, 'container')
digest = run('checksum', unknown).stdout.decode().strip()
assert len(digest) == 64
print('CLI image compatibility and safe rejection checks passed; only temporary files were used.')
PY
python3 - "$cli" "$scratch" <<'PY'
import os
from pathlib import Path
import subprocess
import sys

cli, scratch = sys.argv[1], Path(sys.argv[2])
environment = os.environ.copy()
environment['XDG_STATE_HOME'] = str(scratch / 'private-state')
def run(*args, success=True):
    result = subprocess.run([cli, *map(str,args)], env=environment, text=True, capture_output=True)
    assert (result.returncode == 0) == success, (args,result.stderr)
    return result
version = run('--version').stdout.split()[1]
initial = run('diagnostics').stdout
assert version in initial and 'No saved activity' in initial
report = scratch / 'diagnostics Ω report.txt'
run('diagnostics','--export',report)
assert report.read_text() == initial and report.stat().st_mode & 0o777 == 0o600
run('diagnostics','--export',report,success=False)
assert report.read_text() == initial
run('diagnostics','--clear',success=False)
run('diagnostics','--clear','--confirm','wrong',success=False)
run('diagnostics','--clear','--confirm','clear')
assert 'No saved activity' in run('diagnostics').stdout
unicode_source = scratch / 'installer 日本語 with spaces.img'
unicode_source.write_bytes(bytes(range(256)) * 128)
unicode_copy = scratch / 'verified Ω copy.img'
run('simulate',unicode_source,unicode_copy)
assert unicode_source.read_bytes() == unicode_copy.read_bytes()
print('CLI diagnostics, explicit clearing, export no-clobber and Unicode-path checks passed; private temporary files only.')
PY
