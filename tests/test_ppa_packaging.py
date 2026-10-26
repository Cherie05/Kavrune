#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise real Git exports and dpkg source round trips in temporary files."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("prepare_ppa", ROOT / "scripts/prepare-ppa.py")
PPA = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PPA)
PACKAGE_SPEC = importlib.util.spec_from_file_location("check_package", ROOT / "scripts/check-package.py")
PACKAGE = importlib.util.module_from_spec(PACKAGE_SPEC)
PACKAGE_SPEC.loader.exec_module(PACKAGE)


class SourcePackaging(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="kavrune-ppa-test-")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.repo = self.base / "repo"
        self.repo.mkdir()
        self.env = os.environ.copy()
        for name in tuple(self.env):
            if name.startswith(("GIT_AUTHOR_", "GIT_COMMITTER_")) or name in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR"):
                self.env.pop(name)
        self.env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull,
                        GIT_AUTHOR_NAME="Fixture", GIT_COMMITTER_NAME="Fixture",
                        GIT_AUTHOR_EMAIL="123456+example@users.noreply.github.com",
                        GIT_COMMITTER_EMAIL="123456+example@users.noreply.github.com")
        self.git("init", "-q", "-b", "main")
        (self.repo / "CMakeLists.txt").write_text("project(Kavrune VERSION 0.1.0 LANGUAGES C CXX)\n")
        shutil.copyfile(ROOT / "LICENSE", self.repo / "LICENSE")
        (self.repo / ".gitignore").write_text("/build/\n")
        (self.repo / "script.sh").write_text("#!/bin/sh\nexit 0\n")
        (self.repo / "script.sh").chmod(0o755)
        self.commit()
        self.output = self.base / "prepared"

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.repo), *args], env=self.env, stderr=subprocess.PIPE)

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-q", "-m", "Disposable source fixture")

    def prepare(self, **kwargs):
        return PPA.prepare(self.repo, self.output, "Fixture Maintainer", "packaging@example.invalid", **kwargs)

    def test_export_preserves_files_modes_and_excludes_ignored_builds(self):
        (self.repo / "build").mkdir()
        (self.repo / "build/private.log").write_text("not for publication")
        report = self.prepare()
        source = self.output / report["source_directory"]
        self.assertFalse((source / "build").exists())
        self.assertFalse((source / ".git").exists())
        self.assertEqual((source / "LICENSE").read_bytes(), (ROOT / "LICENSE").read_bytes())
        self.assertTrue((source / "script.sh").stat().st_mode & 0o111)
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o700)
        control = (source / "debian/control").read_text()
        self.assertIn("${shlibs:Depends}", control)
        self.assertIn("BETA DEVELOPMENT PREVIEW", control)
        self.assertIn("udisks2", control)
        self.assertEqual(report["package_version"], "0.1.0-0ubuntu1~beta1~noble1")
        with tarfile.open(self.output / report["orig_archive"]) as archive:
            self.assertFalse(any("/debian/" in n or "/build/" in n for n in archive.getnames()))

    def test_refuses_dirty_tracked_and_untracked_source(self):
        for name in ("CMakeLists.txt", "unreviewed.txt"):
            with self.subTest(name=name):
                path = self.repo / name
                previous = path.read_bytes() if path.exists() else None
                path.write_text("unreviewed changes")
                with self.assertRaisesRegex(ValueError, "clean checkout"):
                    self.prepare()
                self.assertFalse(self.output.exists())
                if previous is None:
                    path.unlink()
                else:
                    path.write_bytes(previous)

    def test_refuses_existing_directory_and_symlink_without_overwrite(self):
        target = self.base / "existing"
        target.mkdir()
        marker = target / "keep"
        marker.write_text("preserve")
        self.output.symlink_to(target, target_is_directory=True)
        with self.assertRaises(FileExistsError):
            self.prepare()
        self.assertEqual(marker.read_text(), "preserve")
        self.output.unlink()
        self.output.mkdir()
        with self.assertRaises(FileExistsError):
            self.prepare()

    def test_refuses_committed_symlinks_and_existing_debian_packaging(self):
        (self.repo / "unsafe").symlink_to("/etc/passwd")
        self.commit()
        with self.assertRaisesRegex(ValueError, "regular files"):
            self.prepare()
        (self.repo / "unsafe").unlink()
        (self.repo / "debian").mkdir()
        (self.repo / "debian/control").write_text("existing package")
        self.commit()
        with self.assertRaisesRegex(ValueError, "already has debian"):
            self.prepare()

    def test_rejects_control_field_injection_and_non_receiving_github_identity(self):
        for name, email in (("Fixture\nInjected: yes", "packaging@example.invalid"),
                            ("Fixture <extra>", "packaging@example.invalid"),
                            ("Fixture", "packaging@example.invalid\nInjected: yes"),
                            ("Fixture", "123456+example@users.noreply.github.com")):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    PPA.prepare(self.repo, self.output, name, email)
                self.assertFalse(self.output.exists())

    def test_preview_version_orders_after_previous_deb_and_before_stable(self):
        old = "0.1.0"
        beta1 = self.prepare()["package_version"]
        beta2 = PPA.prepare(self.repo, self.base / "second", "Fixture", "packaging@example.invalid", beta=2)["package_version"]
        for left, right in ((old, beta1), (beta1, beta2), (beta2, "0.1.0-0ubuntu1")):
            subprocess.run(["dpkg", "--compare-versions", left, "lt", right], check=True)

    def test_real_source_package_build_and_extraction(self):
        self.assertIsNotNone(shutil.which("dpkg-buildpackage"), "dpkg-dev is required; do not silently skip packaging acceptance")
        report = self.prepare()
        source = self.output / report["source_directory"]
        completed = subprocess.run(["dpkg-buildpackage", "-S", "-sa", "-nc", "-us", "-uc"], cwd=source,
                                   text=True, capture_output=True, timeout=60)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        dsc = next(self.output.glob("*.dsc"))
        extracted = self.base / "extracted"
        completed = subprocess.run(["dpkg-source", "--no-check", "-x", str(dsc), str(extracted)],
                                   text=True, capture_output=True, timeout=30)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        for name in ("LICENSE", "CMakeLists.txt", "script.sh", "debian/control", "debian/changelog", "debian/rules"):
            self.assertEqual((source / name).read_bytes(), (extracted / name).read_bytes())
        changes = next(self.output.glob("*_source.changes")).read_text()
        self.assertIn("Architecture: source", changes)
        self.assertIn(report["orig_archive"], changes)
        self.assertNotIn("BEGIN PGP SIGNATURE", changes)
        self.assertFalse(json.loads((self.output / "ppa-preparation.json").read_text())["uploaded"])

    def test_binary_checker_requires_exact_upstream_and_explicit_revision(self):
        revision = "0ubuntu1~beta1~noble1"
        for index, (version, selected_revision) in enumerate((("9.9.9-" + revision, revision), ("0.1.0-" + revision, None))):
            package_root = self.base / f"binary-{index}"
            (package_root / "DEBIAN").mkdir(parents=True)
            (package_root / "DEBIAN/control").write_text(
                f"Package: kavrune\nVersion: {version}\nArchitecture: amd64\n"
                "Maintainer: Fixture <packaging@example.invalid>\nDescription: Disposable version rejection fixture\n")
            package = self.base / f"fixture-{index}.deb"
            subprocess.run(["dpkg-deb", "--build", str(package_root), str(package)], check=True, capture_output=True)
            with self.assertRaisesRegex(ValueError, "version differs"):
                PACKAGE.check(package, self.repo, False, False, None, selected_revision)


if __name__ == "__main__":
    unittest.main()
