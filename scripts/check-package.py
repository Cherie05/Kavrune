#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Inspect a locally built Debian package without installing it or opening drives."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
import xml.etree.ElementTree as ET


def run(*arguments: str) -> str:
    return subprocess.check_output(arguments, text=True, stderr=subprocess.STDOUT, timeout=60)


class Unsupported(RuntimeError):
    pass


def inspect_archive(package: Path) -> None:
    """Reject unsafe/unexpected paths before extracting even a locally built package."""
    total = 0
    for option in ("--fsys-tarfile", "--ctrl-tarfile"):
        with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(
                ["dpkg-deb", option, str(package)], timeout=60)), mode="r:") as archive:
            for member in archive:
                name = PurePosixPath(member.name)
                if name.is_absolute() or ".." in name.parts:
                    raise ValueError(f"Unsafe archive path: {member.name}")
                if not member.isdir() and not member.isfile():
                    raise ValueError(f"Unexpected link or special archive member: {member.name}")
                if member.mode & (stat.S_ISUID | stat.S_ISGID | stat.S_IWOTH):
                    raise ValueError(f"Unsafe archive permissions: {member.name}")
                if option == "--fsys-tarfile" and name.parts and name.parts[0] != "usr":
                    raise ValueError(f"Unexpected install location: {member.name}")
                if any(part in ("systemd", "autostart", "cron.d", "init.d", "dbus-1") for part in name.parts):
                    raise ValueError(f"Unexpected background integration: {member.name}")
                if option == "--ctrl-tarfile" and name.parts and str(name) not in ("control", "md5sums"):
                    raise ValueError(f"Unexpected package control file or installation script: {member.name}")
                total += member.size
                if member.size > 512 * 1024 * 1024 or total > 1024 * 1024 * 1024:
                    raise ValueError("Unexpectedly large package contents.")


def payload_lifecycle(package: Path, extracted: Path, temporary: Path) -> None:
    """Check same-version payload overlay/removal/reinstall; never modify dpkg's database."""
    prefix = temporary / "disposable-prefix"
    sentinel = prefix / "home/example/.local/state/kavrune/activity.log"
    backup = prefix / "home/example/Documents/keep-my-backup.img"
    for path, content in ((sentinel, b"existing local activity\n"), (backup, b"user-owned backup\n")):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    protected = {path: path.read_bytes() for path in (sentinel, backup)}
    files = {path.relative_to(extracted): hashlib.sha256(path.read_bytes()).digest()
             for path in extracted.rglob("*") if path.is_file()}
    directories = sorted((path.relative_to(extracted) for path in extracted.rglob("*")
                          if path.is_dir()), key=lambda path: len(path.parts), reverse=True)

    def intact() -> None:
        for name, digest in files.items():
            if hashlib.sha256((prefix / name).read_bytes()).digest() != digest:
                raise ValueError(f"Stale payload after extraction: {name}")
        for path, content in protected.items():
            if path.read_bytes() != content:
                raise ValueError("The disposable-prefix lifecycle changed user-owned data.")

    for _ in range(2):
        subprocess.run(["dpkg-deb", "-x", str(package), str(prefix)], check=True, timeout=60)
        intact()
    for name in files:
        (prefix / name).unlink()
    for name in directories:
        (prefix / name).rmdir()
    if (prefix / "usr").exists():
        raise ValueError("Package-owned payload remained after manifest-based removal.")
    for path, content in protected.items():
        if path.read_bytes() != content:
            raise ValueError("Payload removal changed user-owned data.")
    subprocess.run(["dpkg-deb", "-x", str(package), str(prefix)], check=True, timeout=60)
    intact()


def check(package: Path, root: Path, self_test: bool, offline: bool, report: Path | None) -> None:
    if run("dpkg-deb", "-f", str(package), "Package").strip() != "kavrune":
        raise ValueError("The package is not Kavrune.")
    version_match = re.search(r"project\(Kavrune VERSION ([0-9.]+)", (root / "CMakeLists.txt").read_text())
    if not version_match:
        raise ValueError("Cannot determine the source version.")
    version = version_match.group(1)
    if run("dpkg-deb", "-f", str(package), "Version").strip() != version:
        raise ValueError("The package version differs from the source version.")
    inspect_archive(package)
    with tempfile.TemporaryDirectory(prefix="kavrune-package-check-") as folder:
        temporary = Path(folder)
        extracted = temporary / "payload"
        subprocess.run(["dpkg-deb", "-x", str(package), str(extracted)], check=True, timeout=60)
        for path in extracted.rglob("*"):
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode) or not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
                raise ValueError(f"Unexpected link or special file: {path.relative_to(extracted)}")
            if mode & (stat.S_ISUID | stat.S_ISGID | stat.S_IWOTH):
                raise ValueError(f"Unsafe package permissions: {path.relative_to(extracted)}")

        docs = extracted / "usr/share/doc/kavrune"
        notices = ("LICENSE", "NOTICE", "TERMS.md", "PRIVACY.md", "DISCLAIMER.md",
                   "SECURITY.md", "THIRD_PARTY_LICENSES.md", "README.md", "CHANGELOG.md",
                   "CONTRIBUTING.md", "CODE_OF_CONDUCT.md", "SUPPORT.md",
                   "PRODUCT_REQUIREMENTS.md", "PRODUCT_STANDARDS.md", "ARCHITECTURE.md",
                   "PRIVACY_AND_LEGAL.md", "TEST_PLAN.md", "RELEASE_CHECKLIST.md")
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
            if run(str(binary), "--version").split()[:2] != ["Kavrune", version]:
                raise ValueError(f"Executable/package version mismatch: {binary.name}")

        for name in ("TERMS.md", "PRIVACY.md", "DISCLAIMER.md", "LICENSE", "THIRD_PARTY_LICENSES.md", "SUPPORT.md"):
            embedded = subprocess.check_output(["gresource", "extract", str(desktop),
                f"/org/kavrune/Kavrune/legal/{name}"])
            if embedded != (root / name).read_bytes():
                raise ValueError(f"Missing or stale offline legal document: {name}")

        for source, installed in (
            (root / "data/org.kavrune.Kavrune.desktop", extracted / "usr/share/applications/org.kavrune.Kavrune.desktop"),
            (root / "data/org.kavrune.Kavrune.metainfo.xml", extracted / "usr/share/metainfo/org.kavrune.Kavrune.metainfo.xml"),
            (root / "data/icon.svg", extracted / "usr/share/icons/hicolor/scalable/apps/org.kavrune.Kavrune.svg"),
        ):
            if source.read_bytes() != installed.read_bytes():
                raise ValueError(f"Missing or stale desktop metadata: {source.name}")

        component = ET.parse(extracted / "usr/share/metainfo/org.kavrune.Kavrune.metainfo.xml").getroot()
        for field, expected in (("id", "org.kavrune.Kavrune"), ("name", "Kavrune"),
                                ("project_license", "MIT"),
                                ("launchable", "org.kavrune.Kavrune.desktop")):
            if component.findtext(field) != expected:
                raise ValueError(f"Inconsistent installed application identity: {field}")
        release = component.find("releases/release")
        if release is None or release.get("version") != version:
            raise ValueError("The AppStream release version differs from the package version.")

        payload_lifecycle(package, extracted, temporary)

        if self_test and not offline:
            # The CLI's self-test uses only generated temporary regular files.
            completed = subprocess.run([str(cli), "self-test", "--json"], text=True,
                                       capture_output=True, check=True, timeout=60)
            result = json.loads(completed.stdout)
            if result.get("passed") is not True or result.get("physical_usb_tested") is not False:
                raise ValueError("The extracted CLI safe self-test did not pass.")
        if offline:
            command = [sys.executable, str(root / "scripts/check-offline.py"),
                       "--cli", str(cli), "--desktop", str(desktop)]
            if report:
                command += ["--report", str(report)]
            result = subprocess.run(command, check=False, timeout=180)
            if result.returncode == 77:
                raise Unsupported("The extracted-package offline test capability is unavailable.")
            if result.returncode:
                raise ValueError("The extracted-package network-denied check failed.")
    print("Package check passed: current notices/guides, offline legal documents, desktop metadata, "
          "ELF hardening and safe permissions; no system installation or physical drive operation.")
    print("Disposable payload extraction, same-version overlay, manifest-based removal and reinstall "
          "preserved user-data sentinels. This does not test dpkg installation, dependency resolution, "
          "cross-version upgrades or clean-machine behavior.")
    if self_test:
        print("Extracted CLI disposable-file self-test passed.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="locally built Kavrune .deb")
    parser.add_argument("--self-test", action="store_true", help="also run the native extracted CLI temporary-file self-test")
    parser.add_argument("--offline", action="store_true", help="test extracted CLI and private-display desktop with process network sockets denied")
    parser.add_argument("--report", type=Path, help="write the --offline evidence and performance report as JSON")
    args = parser.parse_args()
    try:
        if args.report and not args.offline:
            raise ValueError("--report requires --offline.")
        check(args.package.resolve(strict=True), Path(__file__).resolve().parent.parent,
              args.self_test, args.offline, args.report)
    except Unsupported as error:
        print(f"SKIP: {error}", file=sys.stderr)
        return 77
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Package check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
