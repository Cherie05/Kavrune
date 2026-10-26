# Publish Kavrune to GitHub

## Published preview and hosted setup

[Kavrune 0.1.0 Preview 1](https://github.com/Cherie05/Kavrune/releases/tag/v0.1.0-preview.1) was published on 26 October 2026 from commit `b30f1612c9b56fe2b945da35ba164597e7e7903e`. Its Ubuntu 24.04 amd64 package comes from [successful CI run 40267821785](https://github.com/Cherie05/Kavrune/actions/runs/40267821785). The release includes the installer, exact-commit source, original CI manifest, scoped SPDX inventory, offline report, original CI checksums and complete release checksums/provenance. Checksums and provenance are unsigned; publisher-key signing remains separate work.

The official repository is public. Its About description, release homepage and topics are configured. Private vulnerability reporting, dependency alerts/security updates, secret scanning and secret push protection were verified enabled. These hosted settings can change; check their actual state rather than assuming a copied configuration enables them.

The protected `main` branch requires a pull request, resolved review conversations and current successful `Repository hygiene`, `Linux (sanitizers OFF)` and `Linux (sanitizers ON)` checks from GitHub Actions. Protection applies to administrators, with zero mandatory approvals so a sole maintainer can merge their own tested PR. Force pushes and branch deletion are blocked. Publish subsequent source changes through a branch and PR as described below.

Version tags matching `v*` are protected against updates and deletion. Publish a new version instead of moving an existing release tag. Automatic deletion of merged branches and the PR branch-update control are enabled. Existing issue/PR templates, MIT, conduct, contribution and security files remain available. No paid service or signing identity was created.

The release is explicitly a pre-release. Passing CI and configuring GitHub do not complete the physical-media, clean-install, accessibility, independent security, dependency/license or enterprise support acceptance matrix.

## Repository location

The project repository is [Cherie05/Kavrune](https://github.com/Cherie05/Kavrune). Its default branch is `main`, with `origin` configured as `https://github.com/Cherie05/Kavrune.git`. The application remains a 0.1 beta development preview after publication. A configured remote does not mean every local change has been pushed or that hosted security settings have been enabled.

## Update the existing repository

Before editing, start from a clean checkout, update `main` and create a branch with a name describing your change:

```sh
git switch main
git pull --ff-only
git switch -c describe-your-change
```

Before committing or pushing, check the working tree, latest commit and remote. Generated files should remain ignored, and commit author information should be suitable for publication:

```sh
git status --short
git log -1 --oneline
git log -1 --format='%an <%ae>'
git remote -v
python3 scripts/check-repository.py
```

Build and test the changes, review the diff, then stage and commit only the source and documentation intended for publication. With the existing `origin` pointing to the repository you intend to update, push your branch:

```sh
git push -u origin HEAD
```

Open a pull request from that branch into `main` on GitHub. Pushing publishes the local commits and triggers the repository's configured workflows. Check the run for the exact PR revision; an older green run does not validate new changes. Resolve review conversations, wait for all three required checks and merge through the pull request. Do not bypass protection or push directly to the official repository's `main` branch. Check the merged revision's workflow result as well.

## Create a fork or a new repository

These setup steps apply when publishing to a different repository. Create an **empty** repository in the account or organization that should own it. Choose its visibility. Do not initialize it with a README, license or `.gitignore`; those files already exist locally.

Replace `OWNER` below with the intended GitHub user or organization. Use your configured SSH authentication, or the HTTPS URL supplied by GitHub if preferred. For a new checkout without `origin`:

```sh
git remote add origin git@github.com:OWNER/Kavrune.git
git push -u origin main
```

If `origin` already exists, inspect its URL first and use `git remote set-url origin` only when it should point to the different repository. These commands publish existing local commits. Do not force-push to a repository containing unrelated history. No account credentials are shipped with the project.

## Configure the hosted project

- Enable GitHub Actions and check that the repository's workflow actually passes before claiming hosted CI success.
- Protect `main` with a branch rule or ruleset requiring pull requests and the appropriate CI status checks. Review available protection options for the repository's visibility and plan.
- Enable private vulnerability reporting in the repository's security settings and publish a fallback private maintainer contact for [SECURITY.md](../SECURITY.md) and [CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md). See [GitHub's setup instructions](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).
- Enable supported dependency/security features and review alerts; do not treat the generated dependency inventory as a complete vulnerability audit.
- Set a description that identifies the app as a Linux USB utility in development preview. AppStream metadata points to the project's homepage and issue tracker; update those links when publishing a fork under a different owner.

These are hosted settings to review and apply on GitHub; local files cannot enable them.

## Release preparation

Build and test the exact commit intended for release. Run `./scripts/package.sh` to produce local artifacts under the ignored `dist/` directory. Review package contents, third-party notices, dependency versions and checksums before uploading artifacts to a GitHub Release.

For a preview release, explicitly label it as a pre-release and retain the development limitations in the release notes. Keep changes in the unreleased entry of [CHANGELOG.md](../CHANGELOG.md) during development; mark an entry as released only when the release is actually published. Do not commit compiled packages, build trees, downloaded development dependencies, USB images, backups or private logs to Git history.

Complete and record the [release validation matrix](VALIDATION.md) before calling Kavrune production ready. The [MIT License](../LICENSE), [licensing guide](LICENSING.md), [third-party notices](../THIRD_PARTY_LICENSES.md) and [disclaimer](../DISCLAIMER.md) travel with the project; they do not establish completed trademark or dependency clearance.

The [terms](../TERMS.md) and [privacy notice](../PRIVACY.md) also travel with the package and are embedded for offline reading. The package check rejects stale or missing copies. Review the [current standards assessment](STANDARDS_ASSESSMENT.md) and publish working private security/privacy contact details before presenting the project as supported software. Do not remove preview status solely because the automated checks pass.

Packaging now records a process-guarded offline report, direct-runtime SPDX inventory, unsigned release manifest and SHA256SUMS. The manifest fingerprints source files and current artifacts and records the actual Git revision/dirty state. These establish integrity, not publisher trust or reproducibility. A final public release should use a reviewed clean source tree and existing publisher-controlled signing identity; follow [RELEASE_CHECKLIST.md](../RELEASE_CHECKLIST.md). No signing key is generated and no remote publication occurs during the local package command.
