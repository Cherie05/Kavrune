# Publish Kavrune to GitHub

This checkout is prepared as a local Git repository on `main` with the Kavrune source and community files. It does not assume a GitHub owner, configured remote, uploaded repository or enabled account settings. The application remains a 0.1 development preview after publication.

## Create the repository

Sign in to the GitHub account or organization that should own the project and create an **empty** repository named `Kavrune`. Choose its visibility. Do not initialize it with a README, license or `.gitignore`; those files already exist locally.

Before pushing, check the local commit and working tree. Generated files should remain ignored, and commit author information should be suitable for publication:

```sh
git status --short
git log -1 --oneline
git log -1 --format='%an <%ae>'
git remote -v
```

Replace `OWNER` below with your actual GitHub user or organization. Use your configured SSH authentication, or use the HTTPS URL supplied by GitHub if preferred:

```sh
git remote add origin git@github.com:OWNER/Kavrune.git
git push -u origin main
```

These commands publish your existing local commits. If `origin` already exists, inspect its URL first and use `git remote set-url origin` only when it should point to this repository. Do not force-push to a repository containing unrelated history. No remote URL or account credentials are shipped with the project.

## Configure the hosted project

- Enable GitHub Actions and check that the repository's workflow actually passes before claiming hosted CI success.
- Protect `main` with a branch rule or ruleset requiring pull requests and the appropriate CI status checks. Review available protection options for the repository's visibility and plan.
- Enable private vulnerability reporting in the repository's security settings and publish a fallback private maintainer contact for [SECURITY.md](../SECURITY.md) and [CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md). See [GitHub's setup instructions](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).
- Enable supported dependency/security features and review alerts; do not treat the generated dependency inventory as a complete vulnerability audit.
- Set a description that identifies the app as a Linux USB utility in development preview. Configure the real project homepage and repository links in distribution metadata before publishing packages broadly.

These are hosted settings to apply after the repository exists; local files cannot enable them.

## Release preparation

Build and test the exact commit intended for release. Run `./scripts/package.sh` to produce local artifacts under the ignored `dist/` directory. Review package contents, third-party notices, dependency versions and checksums before uploading artifacts to a GitHub Release.

For a preview release, explicitly label it as a pre-release and retain the development limitations in the release notes. Update [CHANGELOG.md](../CHANGELOG.md) only when the release is actually published. Do not commit compiled packages, build trees, downloaded development dependencies, USB images, backups or private logs to Git history.

Complete and record the [release validation matrix](VALIDATION.md) before calling Kavrune production ready. The [MIT License](../LICENSE), [licensing guide](LICENSING.md), [third-party notices](../THIRD_PARTY_LICENSES.md) and [disclaimer](../DISCLAIMER.md) travel with the project; they do not establish completed trademark or dependency clearance.
