#!/usr/bin/env python3
"""Fetch Ubuntu build headers into .deps without root or system installation."""
from pathlib import Path
import os
import re
import subprocess

root = Path(__file__).resolve().parent.parent
prefix = root / '.deps'
downloads = prefix / 'downloads'
downloads.mkdir(parents=True, exist_ok=True)
plan = subprocess.check_output(['apt-get', '-s', '--no-install-recommends', 'install',
                                'libgtk-4-dev', 'libadwaita-1-dev'], text=True)
packages = re.findall(r'^Inst (\S+)', plan, re.M)
if packages:
    subprocess.run(['apt-get', 'download', *packages], cwd=downloads, check=True)
for archive in sorted(downloads.glob('*.deb')):
    subprocess.run(['dpkg-deb', '-x', str(archive), str(prefix)], check=True)
# Development symlinks refer to runtime libraries supplied by the host.
for link in (prefix / 'usr/lib').rglob('*'):
    if link.is_symlink() and not link.exists():
        actual = Path('/usr/lib') / link.relative_to(prefix / 'usr/lib').parent / os.readlink(link)
        if actual.exists():
            link.unlink()
            link.symlink_to(actual.resolve())
arch = subprocess.check_output(['dpkg-architecture', '-qDEB_HOST_MULTIARCH'], text=True).strip()
env = prefix / 'env.sh'
env.write_text('export PKG_CONFIG_PATH="' + str(prefix / 'usr/lib' / arch / 'pkgconfig') + ':' +
               str(prefix / 'usr/share/pkgconfig') + '"\nexport PKG_CONFIG_SYSROOT_DIR="' + str(prefix) +
               '"\nexport PATH="' + str(prefix / 'usr/bin') + ':$PATH"\n')
print(f'Local development headers ready. Run: source {env}')
