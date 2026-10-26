#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the source selected for publication without modifying the repository.

By default inspect indexed files and non-ignored untracked files in the working
tree. With --staged inspect every blob in the Git index, including before the
first commit. This is a focused hygiene check, not a complete secrets scanner
or a licensing/security audit.
With --history also inspect author and committer emails in the complete HEAD
ancestry. Other branches, hosted pull-request refs and caches need a separate audit.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath


REQUIRED = {
    "AGENTS.md",
    "PRODUCT_REQUIREMENTS.md",
    "PRODUCT_STANDARDS.md",
    "ARCHITECTURE.md",
    "PRIVACY_AND_LEGAL.md",
    "TEST_PLAN.md",
    "RELEASE_CHECKLIST.md",
    "docs/STANDARDS_ASSESSMENT.md",
    "docs/REPOSITORY_REVIEW.md",
    "docs/PUBLICATION_PRIVACY.md",
    "tests/test_repository_privacy.py",
    "docs/LAUNCHPAD.md",
    "scripts/prepare-ppa.py",
    "tests/test_ppa_packaging.py",
    ".gitignore",
    ".github/ISSUE_TEMPLATE/bug_report.yml",
    ".github/ISSUE_TEMPLATE/feature_request.yml",
    ".github/ISSUE_TEMPLATE/config.yml",
    ".github/PULL_REQUEST_TEMPLATE.md",
    ".github/VULNERABILITY_REPORT.yml",
    ".github/dependabot.yml",
    ".github/workflows/build.yml",
    "CHANGELOG.md",
    "CMakeLists.txt",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "DISCLAIMER.md",
    "INSTALL.md",
    "LICENSE",
    "NOTICE",
    "PRIVACY.md",
    "README.md",
    "SECURITY.md",
    "SUPPORT.md",
    "TERMS.md",
    "THIRD_PARTY_LICENSES.md",
    "data/org.kavrune.Kavrune.desktop",
    "data/org.kavrune.Kavrune.metainfo.xml",
    "scripts/check-repository.py",
    "scripts/check-package.py",
    "scripts/check-offline.py",
    "scripts/release-manifest.py",
    "scripts/sign-release.sh",
    "include/kavrune/version.hpp.in",
}

GENERATED_SUFFIXES = {
    ".a", ".bin", ".deb", ".dll", ".exe", ".gz", ".img", ".iso",
    ".o", ".pyc", ".qcow2", ".rpm", ".so", ".tar", ".xz", ".zip",
}
IMAGE_SUFFIXES = {".gif", ".ico", ".jpeg", ".jpg", ".png", ".webp"}
PRIVATE_NAMES = {".netrc", "credentials.json", "id_dsa", "id_ecdsa", "id_ed25519", "id_rsa"}
OLD_BRANDING = re.compile(r"luminrove|asterflash", re.IGNORECASE)
PRIVATE_KEY = re.compile(rb"-----BEGIN (?:RSA |EC |DSA |OPENSSH )?PRIVATE KEY-----")
PERSONAL_PATH = re.compile(r"/(?:home|Users)/[^/\s\"'<>]+/|\.codex-antigravity/attachments/")
ACCESS_TOKEN = re.compile(rb"\b(?:gh[opusr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{50,}|AKIA[0-9A-Z]{16})\b")
SELF = "scripts/check-repository.py"
NOREPLY_EMAIL = re.compile(
    rb"(?:[0-9]+\+)?[A-Za-z0-9][A-Za-z0-9-]*(?:\[bot\])?@users\.noreply\.github\.com\Z",
    re.IGNORECASE,
)


def git(root: Path, *arguments: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(root), *arguments], stderr=subprocess.PIPE)


def selected_files(root: Path, staged: bool) -> dict[str, str]:
    """Map publication paths to Git modes without depending on a HEAD commit."""
    files: dict[str, str] = {}
    for entry in git(root, "ls-files", "--stage", "-z").split(b"\0"):
        if not entry:
            continue
        metadata, filename = entry.split(b"\t", 1)
        mode, _oid, stage = metadata.split()
        if stage != b"0":
            raise ValueError("Unresolved merge entries exist; resolve them before publication.")
        files[filename.decode("utf-8", errors="surrogateescape")] = mode.decode("ascii")
    if not staged:
        for filename in git(root, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0"):
            if filename:
                files[filename.decode("utf-8", errors="surrogateescape")] = "100644"
    return files


def path_problem(filename: str) -> str | None:
    path = PurePosixPath(filename)
    name = path.name.lower()
    top = path.parts[0].lower()
    if top in {".deps", "dist", ".cache", ".idea", ".vscode"} or top.startswith(("build", "cmake-build")):
        return "local build, dependency, distribution, or editor file"
    if any(part in {"__pycache__", "CMakeFiles", "_CPack_Packages"} for part in path.parts):
        return "generated cache or package file"
    if path.suffix.lower() in GENERATED_SUFFIXES or name.endswith((".log", ".swp", ".swo")):
        return "generated binary, archive, device image, or local log"
    if name in PRIVATE_NAMES or name.endswith(".key") or (name.startswith(".env") and name != ".env.example"):
        return "credential or environment file"
    if OLD_BRANDING.search(filename):
        return "former application name in path"
    return None


def check(root: Path, staged: bool) -> tuple[int, list[str]]:
    files = selected_files(root, staged)
    errors = [f"Missing publication file: {filename}" for filename in sorted(REQUIRED - files.keys())]
    for filename, mode in sorted(files.items()):
        problem = path_problem(filename)
        if problem:
            errors.append(f"{filename}: {problem}")
            continue
        path = root / filename
        if mode not in {"100644", "100755"} or (not staged and path.is_symlink()):
            errors.append(f"{filename}: symlink, submodule, or unsupported file mode; review before publication")
            continue
        try:
            content = git(root, "show", f":{filename}") if staged else path.read_bytes()
        except (OSError, subprocess.CalledProcessError):
            errors.append(f"{filename}: cannot read selected content")
            continue
        if content.startswith(b"\x7fELF"):
            errors.append(f"{filename}: compiled ELF binary")
            continue
        if b"\0" in content:
            if path.suffix.lower() not in IMAGE_SUFFIXES:
                errors.append(f"{filename}: unexpected binary file")
            continue
        if PRIVATE_KEY.search(content):
            errors.append(f"{filename}: private key material")
        if ACCESS_TOKEN.search(content):
            errors.append(f"{filename}: possible access token; inspect without publishing it")
        try:
            text = content.decode("utf-8")
        except UnicodeDecodeError:
            errors.append(f"{filename}: non-UTF-8 text; review encoding before publication")
            continue
        # This checker embeds branding/path patterns. Ignore only those pattern
        # definitions; still inspect the file for actual keys and access tokens.
        if filename != SELF and OLD_BRANDING.search(text):
            errors.append(f"{filename}: former application name in text")
        if filename != SELF and PERSONAL_PATH.search(text):
            errors.append(f"{filename}: personal home or attachment path")
    return len(files), errors


def check_history(root: Path) -> tuple[int, list[str]]:
    """Check HEAD ancestry, never printing the rejected address."""
    try:
        if git(root, "rev-parse", "--is-shallow-repository").strip() != b"false":
            raise ValueError("Full Git history is required for identity checks; fetch without a depth limit.")
        records = git(root, "log", "HEAD", "--format=%H%x00%ae%x00%ce").splitlines()
    except subprocess.CalledProcessError as error:
        raise ValueError("Cannot inspect identities without complete committed Git history.") from error
    errors = []
    for record in records:
        fields = record.split(b"\0")
        if len(fields) != 3:
            raise ValueError("Cannot parse Git identity metadata safely.")
        oid, author, committer = fields
        for role, email in (("author", author), ("committer", committer)):
            if email.lower() != b"noreply@github.com" and not NOREPLY_EMAIL.fullmatch(email):
                errors.append(
                    f"{oid[:12].decode('ascii')}: {role} email must use a GitHub noreply identity "
                    "(address omitted); review Git identity settings before publication."
                )
    return len(records), errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staged", action="store_true", help="inspect all Git index contents rather than working files")
    parser.add_argument("--history", action="store_true", help="require complete HEAD ancestry with GitHub noreply author and committer emails")
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    try:
        count, errors = check(root, arguments.staged)
        if arguments.history:
            commits, identity_errors = check_history(root)
            errors.extend(identity_errors)
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"Repository check could not run: {error}", file=sys.stderr)
        return 2
    if errors:
        print("Repository publication check failed:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        return 1
    selection = "Git index" if arguments.staged else "working tree publication candidates"
    print(f"Repository publication check passed: {count} files in {selection}.")
    print("No generated artifacts, former branding, or detected private material were selected.")
    if arguments.history:
        print(f"HEAD identity check passed: {commits} commits; both author and committer use GitHub noreply.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
