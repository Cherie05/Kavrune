#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare a private, unsigned Ubuntu source tree from a clean committed checkout.

No keys, signatures, uploads, package installation or physical devices are used.
The supplied maintainer identity will be public if the resulting package is uploaded.
"""
from __future__ import annotations

import argparse
from email.utils import formatdate
import hashlib
import io
import json
import lzma
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
EMAIL = re.compile(r"[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+\Z")
CONTROL = """Source: kavrune
Section: utils
Priority: optional
Maintainer: {identity}
Build-Depends: debhelper-compat (= 13), cmake (>= 3.22), g++, pkg-config,
 libssl-dev (>= 3), libglib2.0-dev, libglib2.0-dev-bin,
 libgtk-4-dev (>= 4.12), libadwaita-1-dev (>= 1.4),
 python3, dbus-x11, xvfb, xauth, libseccomp2, qemu-system-x86, seabios
Standards-Version: 4.6.2
Rules-Requires-Root: no
Homepage: https://github.com/Cherie05/Kavrune
Vcs-Git: https://github.com/Cherie05/Kavrune.git
Vcs-Browser: https://github.com/Cherie05/Kavrune

Package: kavrune
Architecture: amd64
Depends: ${{shlibs:Depends}}, ${{misc:Depends}}, udisks2 (>= 2.9), polkitd,
 dosfstools, exfatprogs, e2fsprogs
Description: offline Linux USB utility - beta development preview
 Create USB media from supported Linux images with mandatory readback
 verification, format removable storage, save raw backups and check data.
 .
 BETA DEVELOPMENT PREVIEW: under active development, provided without
 warranty. Storage operations can erase data; keep independent backups.
"""
RULES = """#!/usr/bin/make -f
export DEB_BUILD_MAINT_OPTIONS = hardening=+all

%:
\tdh $@ --buildsystem=cmake

override_dh_auto_configure:
\tdh_auto_configure -- -DBUILD_TESTING=ON -DKAVRUNE_BUILD_DESKTOP=ON -DKAVRUNE_SANITIZERS=OFF

override_dh_auto_test:
\tdh_auto_test -- ARGS="--output-on-failure --timeout 90"

override_dh_compress:
\tdh_compress -X.md -XLICENSE -XNOTICE

override_dh_icons:
\tdh_icons --no-scripts
"""


def git(root: Path, *args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.PIPE)


def validate_identity(name: str, email: str) -> str:
    if not name.strip() or len(name) > 100 or any(ord(c) < 32 or c in "<>" for c in name):
        raise ValueError("Use a single-line maintainer name without angle brackets or control characters.")
    if len(email) > 254 or not EMAIL.fullmatch(email):
        raise ValueError("Use a single working email address without spaces, brackets or newlines.")
    if email.lower().endswith(("@users.noreply.github.com", "@github.com")):
        raise ValueError("Package contact must receive mail; use your chosen public project address.")
    return f"{name.strip()} <{email}>"


def prepare(root: Path, output: Path, name: str, email: str, beta: int = 1) -> dict:
    """Export committed files only; a fresh output directory is always required."""
    identity = validate_identity(name, email)
    if beta < 1 or beta > 999999:
        raise ValueError("Beta number must be between 1 and 999999.")
    if git(root, "status", "--porcelain", "--untracked-files=normal").strip():
        raise ValueError("Commit and review intended changes first; source packaging requires a clean checkout.")
    oid = git(root, "rev-parse", "HEAD").decode().strip()
    cmake = git(root, "show", "HEAD:CMakeLists.txt").decode()
    match = re.search(r"project\(Kavrune\s+VERSION\s+(\d+\.\d+\.\d+)\b", cmake)
    if not match:
        raise ValueError("Cannot read the upstream version from committed CMakeLists.txt.")
    upstream = match[1]
    version = f"{upstream}-0ubuntu1~beta{beta}~noble1"
    archive = git(root, "archive", "--format=tar", f"--prefix=kavrune-{upstream}/", oid)
    # Validate the complete archive before creating any output, including links.
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        members = tar.getmembers()
        for member in members:
            path = PurePosixPath(member.name)
            if path.is_absolute() or ".." in path.parts or path.parts[0] != f"kavrune-{upstream}":
                raise ValueError("Unexpected source archive path.")
            if not (member.isdir() or member.isfile()):
                raise ValueError("Source exports must contain regular files and directories only.")
            if len(path.parts) > 1 and path.parts[1] == "debian":
                raise ValueError("Upstream already has debian packaging; review it instead of replacing it.")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.mkdir(mode=0o700)  # Refuse existing files, directories and symlinks.
        for member in members:
            target = output / member.name
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open("xb") as stream:
                    stream.write(tar.extractfile(member).read())
                target.chmod(0o755 if member.mode & 0o111 else 0o644)
    orig = output / f"kavrune_{upstream}.orig.tar.xz"
    orig.write_bytes(lzma.compress(archive))
    source = output / f"kavrune-{upstream}"
    debian = source / "debian"
    (debian / "source").mkdir(parents=True)
    (debian / "source/format").write_text("3.0 (quilt)\n")
    (debian / "control").write_text(CONTROL.format(identity=identity))
    (debian / "rules").write_text(RULES)
    (debian / "rules").chmod(0o755)
    date = formatdate(int(git(root, "show", "-s", "--format=%ct", oid)), localtime=False)
    (debian / "changelog").write_text(
        f"kavrune ({version}) noble; urgency=medium\n\n"
        "  * Beta development preview; no stable-release qualification is claimed.\n"
        f"  * Build from reviewed upstream commit {oid}.\n"
        "  * Preserve mandatory verification, system-drive protection and offline notices.\n\n"
        f" -- {identity}  {date}\n"
    )
    license_text = (source / "LICENSE").read_text().splitlines()
    copyright_text = (
        "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/\n"
        "Upstream-Name: Kavrune\n"
        "Source: https://github.com/Cherie05/Kavrune\n\n"
        "Files: *\nCopyright: 2026 Kavrune contributors\nLicense: MIT\n"
        + "\n".join(" " + line if line else " ." for line in license_text[4:]) + "\n"
    )
    (debian / "copyright").write_text(copyright_text)
    report = {
        "schema_version": 1, "record_type": "unsigned_ppa_source_preparation",
        "source_commit": oid, "source_tree": git(root, "rev-parse", oid + "^{tree}").decode().strip(),
        "upstream_version": upstream, "package_version": version,
        "ubuntu_series": "noble", "architecture": "amd64",
        "source_directory": source.name, "orig_archive": orig.name,
        "orig_archive_sha256": hashlib.sha256(orig.read_bytes()).hexdigest(),
        "source_files": sum(m.isfile() for m in members),
        "signed": False, "uploaded": False,
        "scope": "Prepared source only; build, signing, Launchpad acceptance and installation require separate checks.",
    }
    (output / "ppa-preparation.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--maintainer-name", required=True, help="name approved for public package metadata")
    parser.add_argument("--maintainer-email", required=True, help="working address approved for public package metadata")
    parser.add_argument("--beta", type=int, default=1, help="increase for every changed upload (default: 1)")
    parser.add_argument("--output", type=Path, required=True, help="new private output directory; must not exist")
    args = parser.parse_args()
    try:
        subprocess.run([sys.executable, str(ROOT / "scripts/check-repository.py"), "--staged", "--history"], check=True)
        report = prepare(ROOT, args.output.absolute(), args.maintainer_name, args.maintainer_email, args.beta)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"PPA preparation failed: {error}", file=sys.stderr)
        return 1
    print(f"Prepared {report['package_version']} for Ubuntu 24.04 (noble), amd64.")
    print(f"Source: {args.output / report['source_directory']}")
    print("Unsigned and not uploaded. Follow docs/LAUNCHPAD.md for signing and publication.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
