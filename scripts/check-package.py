#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Inspect a locally built Debian package without installing it or opening drives."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import stat
import subprocess
import sys
import tempfile


def run(*arguments: str) -> str:
    return subprocess.check_output(arguments, text=True, stderr=subprocess.STDOUT, timeout=60)


def check(package: Path, root: Path, self_test: bool) -> None:
    if run("dpkg-deb", "-f", str(package), "Package").strip() != "kavrune":
        raise ValueError("The package is not Kavrune.")
    with tempfile.TemporaryDirectory(prefix="kavrune-package-check-") as folder:
        extracted = Path(folder)
        subprocess.run(["dpkg-deb", "-x", str(package), folder], check=True)
        for path in extracted.rglob("*"):
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode) or not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
                raise ValueError(f"Unexpected link or special file: {path.relative_to(extracted)}")
            if mode & (stat.S_ISUID | stat.S_ISGID | stat.S_IWOTH):
                raise ValueError(f"Unsafe package permissions: {path.relative_to(extracted)}")

        docs = extracted / "usr/share/doc/kavrune"
        notices = ("LICENSE", "NOTICE", "TERMS.md", "PRIVACY.md", "DISCLAIMER.md",
                   "SECURITY.md", "THIRD_PARTY_LICENSES.md", "README.md", "CHANGELOG.md")
        for name in notices:
            if (docs / name).read_bytes() != (root / name).read_bytes():
                raise ValueError(f"Missing or stale packaged notice: {name}")
        for path in (root / "docs").rglob("*"):
            if path.is_file() and path.read_bytes() != (docs / "docs" / path.relative_to(root / "docs")).read_bytes():
                raise ValueError(f"Missing or stale packaged guide: {path.name}")

        desktop = extracted / "usr/bin/kavrune"
        cli = extracted / "usr/bin/kavrune-cli"
        for binary in (desktop, cli):
            if not binary.stat().st_mode & stat.S_IXUSR or binary.read_bytes()[:4] != b"\x7fELF":
                raise ValueError(f"Missing executable ELF: {binary.name}")
            dynamic = run("readelf", "-dW", str(binary))
            segments = run("readelf", "-lW", str(binary))
            if "(RPATH)" in dynamic or "(RUNPATH)" in dynamic:
                raise ValueError(f"Runtime search path retained in {binary.name}")
            if "GNU_RELRO" not in segments or "BIND_NOW" not in dynamic or "PIE" not in dynamic:
                raise ValueError(f"Expected ELF hardening missing in {binary.name}")
            stack = next((line for line in segments.splitlines() if "GNU_STACK" in line), "")
            if not stack or "E" in "".join(stack.split()[6:-1]):
                raise ValueError(f"Executable or unknown stack in {binary.name}")

        for name in ("TERMS.md", "PRIVACY.md", "DISCLAIMER.md", "LICENSE"):
            embedded = subprocess.check_output(["gresource", "extract", str(desktop),
                f"/org/kavrune/Kavrune/legal/{name}"])
            if embedded != (root / name).read_bytes():
                raise ValueError(f"Missing or stale offline legal document: {name}")

        for source, installed in (
            (root / "data/org.kavrune.Kavrune.desktop", extracted / "usr/share/applications/org.kavrune.Kavrune.desktop"),
            (root / "data/org.kavrune.Kavrune.metainfo.xml", extracted / "usr/share/metainfo/org.kavrune.Kavrune.metainfo.xml"),
        ):
            if source.read_bytes() != installed.read_bytes():
                raise ValueError(f"Missing or stale desktop metadata: {source.name}")

        if self_test:
            # The CLI's self-test uses only generated temporary regular files.
            completed = subprocess.run([str(cli), "self-test", "--json"], text=True,
                                       capture_output=True, check=True, timeout=60)
            result = json.loads(completed.stdout)
            if result.get("passed") is not True or result.get("physical_usb_tested") is not False:
                raise ValueError("The extracted CLI safe self-test did not pass.")
    print("Package check passed: current notices/guides, offline legal documents, desktop metadata, "
          "ELF hardening and safe permissions; no installation or physical drive access.")
    if self_test:
        print("Extracted CLI disposable-file self-test passed.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="locally built Kavrune .deb")
    parser.add_argument("--self-test", action="store_true", help="also run the native extracted CLI temporary-file self-test")
    args = parser.parse_args()
    try:
        check(args.package.resolve(strict=True), Path(__file__).resolve().parent.parent, args.self_test)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Package check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
