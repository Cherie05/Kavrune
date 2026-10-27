#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise real Git metadata in disposable repositories, never personal data."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("repository_check", ROOT / "scripts/check-repository.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)
SAFE = "123456+example@users.noreply.github.com"
PRIVATE = "fixture@example.invalid"


class IdentityChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="kavrune-git-privacy-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "repo"
        self.root.mkdir()
        self.env = os.environ.copy()
        for key in tuple(self.env):
            if key.startswith(("GIT_AUTHOR_", "GIT_COMMITTER_")) or key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR"):
                self.env.pop(key)
        self.env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
        self.git("init", "-q", "-b", "main")
        self.serial = 0

    def git(self, *args, env=None):
        return subprocess.check_output(["git", "-C", str(self.root), *args], env=env or self.env, stderr=subprocess.PIPE).decode().strip()

    def commit(self, author=SAFE, committer=SAFE):
        self.serial += 1
        (self.root / "fixture.txt").write_text(str(self.serial))
        self.git("add", "fixture.txt")
        env = self.env | {
            "GIT_AUTHOR_NAME": "Fixture Author", "GIT_AUTHOR_EMAIL": author,
            "GIT_COMMITTER_NAME": "Fixture Committer", "GIT_COMMITTER_EMAIL": committer,
        }
        self.git("commit", "-q", "-m", "Disposable fixture", env=env)
        return self.git("rev-parse", "HEAD")[:12]

    def test_accepts_current_legacy_and_automation_noreply(self):
        for email in (SAFE, "example@users.noreply.github.com", "41898282+github-actions[bot]@users.noreply.github.com", "noreply@github.com"):
            self.commit(author=email, committer=email)
        count, errors = CHECK.check_history(self.root)
        self.assertEqual(count, 4)
        self.assertEqual(errors, [])

    def test_rejects_private_author_without_repeating_address(self):
        oid = self.commit(author=PRIVATE)
        count, errors = CHECK.check_history(self.root)
        self.assertEqual(count, 1)
        self.assertEqual(len(errors), 1)
        self.assertIn(oid, errors[0])
        self.assertIn("author email", errors[0])
        self.assertNotIn(PRIVATE, errors[0])

    def test_rejects_private_committer_with_safe_author(self):
        self.commit(committer=PRIVATE)
        count, errors = CHECK.check_history(self.root)
        self.assertEqual(count, 1)
        self.assertEqual(len(errors), 1)
        self.assertIn("committer email", errors[0])
        self.assertNotIn(PRIVATE, errors[0])

    def test_checks_ancestors_when_latest_identity_is_safe(self):
        old = self.commit(author=PRIVATE)
        self.commit()
        count, errors = CHECK.check_history(self.root)
        self.assertEqual(count, 2)
        self.assertEqual(len(errors), 1)
        self.assertIn(old, errors[0])

    def test_rejects_shallow_history(self):
        self.commit(author=PRIVATE)
        self.commit()
        shallow = self.root.parent / "shallow"
        self.git("-c", "protocol.file.allow=always", "clone", "-q", "--depth=1", self.root.as_uri(), str(shallow))
        with self.assertRaisesRegex(ValueError, "Full Git history"):
            CHECK.check_history(shallow)

    def test_rejects_missing_commit_history(self):
        with self.assertRaisesRegex(ValueError, "complete committed"):
            CHECK.check_history(self.root)

    def test_unrelated_branch_is_outside_head_scope(self):
        self.commit()
        self.git("switch", "-q", "-c", "other")
        self.commit(author=PRIVATE)
        self.git("switch", "-q", "main")
        count, errors = CHECK.check_history(self.root)
        self.assertEqual(count, 1)
        self.assertEqual(errors, [])

    def test_rejects_address_hidden_before_noreply_suffix(self):
        self.commit(author=PRIVATE + "@users.noreply.github.com")
        _, errors = CHECK.check_history(self.root)
        self.assertEqual(len(errors), 1)
        self.assertNotIn(PRIVATE, errors[0])


class PublicationContentChecks(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="kavrune-source-privacy-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.env = os.environ.copy()
        for name in tuple(self.env):
            if name.startswith(("GIT_AUTHOR_", "GIT_COMMITTER_")) or name in (
                "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR"
            ):
                self.env.pop(name)
        self.env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
        self.git("init", "-q")
        (self.root / ".gitignore").write_bytes((ROOT / ".gitignore").read_bytes())

    def git(self, *args):
        return subprocess.check_output(
            ["git", "-C", str(self.root), *args], env=self.env, stderr=subprocess.PIPE
        )

    def check(self, staged):
        # Required project documents are independent of these content fixtures.
        with mock.patch.object(CHECK, "REQUIRED", set()):
            return CHECK.check(self.root, staged)

    def test_private_key_markers_are_rejected_without_echoing_content(self):
        path = self.root / "fixture.txt"
        for kind in (b"RSA PRIVATE KEY", b"ENCRYPTED PRIVATE KEY", b"OPENSSH PRIVATE KEY", b"PGP PRIVATE KEY BLOCK"):
            with self.subTest(kind=kind):
                payload = b"-----BEGIN " + kind + b"-----\nfixture material only\n"
                path.write_bytes(payload)
                self.git("add", "fixture.txt")
                for staged in (False, True):
                    _, errors = self.check(staged)
                    self.assertTrue(any("private key material" in error for error in errors))
                    self.assertNotIn("fixture material only", "\n".join(errors))

    def test_confirmation_link_hidden_by_unstaged_edit_is_still_rejected(self):
        path = self.root / "fixture.txt"
        link = b"https://launchpad.net/" + b"token/" + b"inert-fixture-only"
        path.write_bytes(link)
        self.git("add", "fixture.txt")
        path.write_text("Reviewed source without account links.\n")
        _, errors = self.check(True)
        self.assertTrue(any("account-confirmation link" in error for error in errors))
        self.assertNotIn(link.decode(), "\n".join(errors))
        self.assertEqual(self.check(False)[1], [])

    def test_force_adding_ignored_keys_and_uploads_cannot_bypass_guard(self):
        names = (
            "export.asc", "pubring.kbx", "key.gpg", "agent.gpg-agent-info", "key.pem", "key.p12", "key.ppk",
            "public.pub", ".gnupg/private-keys-v1.d/record", ".ssh/id_ed25519",
            "openpgp-revocs.d/record", "id_ecdsa_sk", "package.dsc", "package.changes",
            "package.buildinfo", "package.upload", "package.ddeb",
        )
        for name in names:
            with self.subTest(name=name):
                path = self.root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("Inert fixture, never a real key or credential.\n")
                self.git("check-ignore", "-q", "--", name)
                self.assertEqual(self.check(False)[1], [])
                self.git("add", "-f", "--", name)
                _, errors = self.check(True)
                self.assertTrue(any(error.startswith(name + ":") for error in errors))
                self.git("rm", "--cached", "-f", "--", name)
                path.unlink()

    def test_documented_signing_commands_and_placeholders_remain_allowed(self):
        (self.root / "guide.txt").write_text(
            "gpg --decrypt launchpad-confirmation.asc\n"
            "gpg --keyserver hkps://keyserver.ubuntu.com --send-keys YOUR_FULL_FINGERPRINT\n"
        )
        self.git("add", "guide.txt")
        self.assertEqual(self.check(False)[1], [])
        self.assertEqual(self.check(True)[1], [])


if __name__ == "__main__":
    unittest.main()
