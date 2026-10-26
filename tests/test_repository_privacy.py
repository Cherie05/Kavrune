#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise real Git metadata in disposable repositories, never personal data."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

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


if __name__ == "__main__":
    unittest.main()
