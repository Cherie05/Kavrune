# Privacy when publishing source

Public Git repositories expose file contents, older versions, commit author and committer identities, tags, pull requests and hosted build records. A clean working tree and an empty secret-alert list do not establish that historical metadata is private. Public account names and GitHub noreply identities still identify contributors.

## Before a commit or push

Use the GitHub-provided noreply address from your account's email settings for both author and committer emails. Set it locally for this checkout; do not invent an address or publish a personal contact to satisfy package metadata. Review both effective identities privately:

```sh
git var GIT_AUTHOR_IDENT
git var GIT_COMMITTER_IDENT
```

If either is wrong, set `git config --local user.email` to your actual GitHub noreply address and check environment overrides as well. Review existing commits separately: changing the setting does not change past author or committer data. Do not paste private identity output into public issues or logs.

Stage only intended changes and review the effective identities before committing. After creating a commit, check the selected source and complete current branch ancestry before every push:

```sh
python3 tests/test_repository_privacy.py
python3 scripts/check-repository.py --staged --history
```

The history check accepts GitHub user/bot noreply addresses and GitHub's own `noreply@github.com` committer identity. It reports the commit and affected role without printing a rejected address. It checks both fields, earlier ancestors and the complete history; it refuses a shallow clone. CI fetches full history for this check. Ordinary publication still uses a protected-branch PR and required checks.

The guard checks HEAD ancestry. It does not scan every remote branch, GitHub's retained PR refs, Actions run metadata, release payload, screenshot metadata or cached/deleted object. Audit those surfaces separately before claiming that a public project is free of personal information. It does not anonymize account/display names or replace a general secrets scanner.

CI runs after a push, so a rejected commit may already be public. Run the local check after committing and before uploading; an earlier check cannot inspect a commit that does not exist yet. Audit other refs separately if pushing multiple branches or tags. Cloning does not automatically install a local Git hook.

## Existing public information

Do not merge old history back into a cleaned repository. After an approved history cleanup, use a fresh clone for ordinary development. Preserve unpushed work privately and carefully reapply intended source changes rather than merging an old branch. This avoids reintroducing old identity metadata.

Canonical history was normalized on 26 October 2026 with explicit maintainer approval. The metadata-only replacement retained identical file trees for all 16 rewritten commits. Branch and version-tag protections were restored. Original preview installer/source asset bytes and checksums remain unchanged; [release notices](https://github.com/Cherie05/Kavrune/releases/tag/v0.1.0-preview.1) explain the provenance bridge. Retained legacy PR refs and cached copies require GitHub-side handling; a rewrite alone is not a complete purge.

GitHub Actions run metadata can retain author/committer email values independently of the branch. Archive required evidence privately and remove affected hosted records as part of an approved cleanup. Do not upload raw backup metadata publicly. Any public evidence export must omit personal identity fields and accurately identify historical versus fresh validation.

GitHub documents the [effects of rewriting history and the limits of cache, PR and clone removal](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository). GitHub Support eligibility and removal decisions belong to GitHub; existing external clones cannot be erased by the maintainer.
