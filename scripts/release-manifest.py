#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Record source and artifact integrity without uploading data or signing anything."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('artifacts', type=Path, nargs='+')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    version = re.search(r'project\(Kavrune VERSION ([0-9.]+)', (root / 'CMakeLists.txt').read_text()).group(1)
    paths = set(git(root, 'ls-files', '-z').split(b'\0'))
    paths.update(git(root, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0'))
    digest = hashlib.sha256()
    count = 0
    for raw in sorted(paths - {b''}):
        name = raw.decode('utf-8')
        path = root / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f'Cannot fingerprint source: {name}')
        digest.update(raw + b'\0' + hashlib.sha256(path.read_bytes()).digest())
        count += 1
    artifacts = []
    for path in args.artifacts:
        with path.open('rb') as stream:
            sha256 = hashlib.file_digest(stream, 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else hashlib.sha256(stream.read()).hexdigest()
        artifacts.append({'name': path.name, 'bytes': path.stat().st_size, 'sha256': sha256})
    if len({item['name'] for item in artifacts}) != len(artifacts):
        raise ValueError('Artifact names must be unique.')
    document = {
        'schema_version': 1,
        'product': 'Kavrune', 'version': version, 'release_status': 'development-preview',
        'source': {'git_revision': git(root, 'rev-parse', 'HEAD').decode().strip(),
                   'working_tree_dirty': bool(git(root, 'status', '--porcelain')),
                   'files': count, 'sha256': digest.hexdigest(),
                   'fingerprint_format': 'sorted UTF-8 path + NUL + raw SHA256(file bytes), concatenated and SHA256 hashed'},
        'artifacts': artifacts,
        'authentication': 'Unsigned integrity record. Verify a separately authenticated publisher signature before trusting a release.',
        'validation_scope': 'See packaged BUILD_RESULTS and RELEASE_CHECKLIST. This manifest is not an attestation of production qualification, licensing, vulnerability review or reproducibility.',
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2) + '\n')
    print(f'Release integrity manifest written: {args.output.name}')


if __name__ == '__main__':
    main()
