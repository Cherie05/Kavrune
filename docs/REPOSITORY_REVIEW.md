# Repository notices and publication review — 26 October 2026

**Status: beta development preview, under active development.** This is a focused review of repository files, user instructions and GitHub settings. It is not an independent storage security audit, legal opinion, trademark clearance or guarantee against data loss.

## Publication hygiene update — 27 October 2026

The 87 selected source files and reachable local Git history were reviewed again. Gitleaks 8.30.1 reported no detected secrets in the publication snapshot or reachable history; targeted checks found no private-key blocks, account-confirmation links, personal home paths or the approved package-contact address in the source snapshot. The seven tracked PNG files contained no text/EXIF metadata chunks. These are scoped observations, not a guarantee that no private information exists on every hosted or external surface.

The guard now rejects armored OpenPGP/encrypted private-key markers and Launchpad confirmation links without echoing their contents. Expanded ignore rules exclude key exports/keyrings and source-upload artifacts; force-added files remain subject to the guard. All 12 disposable privacy tests and eight source-packaging tests passed locally. GitHub secret scanning and push protection were enabled with no open secret-scanning alerts at this check.

Seven stale ignored build/audit folders were archived outside the checkout under owner-only access. The primary build, local SDK, current installer and immutable accepted source archives remain local and excluded from Git; required project, license, security, contribution and GitHub files remain in the source selection.

Closed GitHub PRs [#1](https://github.com/Cherie05/Kavrune/pull/1) and [#2](https://github.com/Cherie05/Kavrune/pull/2) still retain earlier personal commit metadata. Their retained refs and cached copies are outside a normal branch push; the earlier canonical-history cleanup did not erase them. No new history rewrite or claim of complete GitHub-side removal is made. See [publication privacy](PUBLICATION_PRIVACY.md).

## User notices and instructions

- [README](../README.md), [NOTICE](../NOTICE), [DISCLAIMER](../DISCLAIMER.md), [TERMS](../TERMS.md), [SUPPORT](../SUPPORT.md) and [CONTRIBUTING](../CONTRIBUTING.md) identify the beta/development status. Notices describe unfinished features, failures, permanent data loss, broken/corrupted media and device/system damage.
- The original [MIT License](../LICENSE) retains its permissions, warranty disclaimer and liability language. Explanatory notices qualify exclusions by applicable law and add no license restrictions or forced agreement.
- [INSTALL](../INSTALL.md) gives user instructions for the published installer, checksums, dependencies, normal-user launch, a safe test, manual updates, removal and common failures. Users need no Git, compiler or GitHub account.
- [Privacy](../PRIVACY.md), [third-party notices](../THIRD_PARTY_LICENSES.md), [security reporting](../SECURITY.md), [conduct](../CODE_OF_CONDUCT.md), contribution and support documents are present. No contact details, signing identity or response-time promise are fabricated.
- Current source builds embed the updated legal notices and install the new guide. Historical release assets remain tied to their original tagged source; later repository text does not silently replace those binaries.

## GitHub and source controls

The official repository was verified public, with About metadata, download homepage and topics configured. Private vulnerability reporting, dependency alerts/security updates, secret scanning and secret push protection were verified enabled. The current secret-scanning and Dependabot alert lists were empty at the time of the check; this does not establish a complete dependency or security audit.

`main` requires a pull request, resolved conversations and the three existing GitHub Actions checks, including normal and sanitizer builds. Administrators are subject to protection; force pushes and deletion are blocked. Version tags matching `v*` are protected against updates and deletion. Workflows use pinned checkout/upload actions with read-only default permissions. The status of these hosted settings must be rechecked when publishing; copying files does not enable them in forks.

Bug/feature issue forms, private vulnerability form, issue contact links, a PR template and Dependabot configuration are present. `.gitignore` covers build trees, local dependencies, generated packages, disk images, credentials, private local key storage, diagnostics, crash dumps and editor caches. Source publication checks reject detected private material, unexpected binaries, generated artifacts and personal paths; heuristic checks can miss issues and are not proof that no secret exists.

The [publishing instructions](PUBLISHING.md) use a feature branch and tested PR for updates, matching the official repository's protected `main` policy. The private vulnerability form's required fields and minimum proof-of-concept length match [GitHub's documented private-report form support](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository); no test vulnerability report was submitted.

CI dependency installation retains the GitHub runner's existing Ubuntu HTTPS archive fallbacks and archive signing keys while omitting its slow Azure HTTP mirror entry. Download retries and socket timeouts are explicit; all required checks remain enabled. This changes only ephemeral CI runners, not a user's package sources. The mirror-file format follows [GitHub's runner image configuration](https://github.com/actions/runner-images/blob/main/images/ubuntu/scripts/build/configure-apt-sources.sh).

The cleaned baseline `main` revision `3f18ec5711f6dc5496b73136ddda823327fd6826` passed [hosted CI](https://github.com/Cherie05/Kavrune/actions/runs/40354783350). The approved metadata-only history normalization preserved every file tree and restored branch/tag protections. The [publication privacy guide](PUBLICATION_PRIVACY.md) explains the new author-and-committer check, its HEAD-only scope and remaining legacy GitHub PR/cache handling. The follow-up privacy gate must pass its own required checks. See [BUILD_RESULTS](BUILD_RESULTS.md) for scoped software and package evidence.

## Remaining work and deliberate limits

- Launchpad account and the public `arunvpp/kavrune-beta` PPA were observed active on 26 October 2026, with no published source packages or registered upload key. Source packaging and an [end-to-end publication guide](LAUNCHPAD.md) are provided; actual signing, accepted upload, published target build and clean-VM APT installation are separate gates. The historical GitHub release's local `.deb` remains available.
- Publisher signing remains pending; current release checksum/provenance records are unsigned and provide integrity, not independent publisher authentication.
- Real USB authorization, formatting, backup/restore, removal/power-loss recovery, clean installation, supported distro/firmware acceptance, complete accessibility and independent parser/storage security review remain required before production qualification.
- Direct-runtime SPDX inventory and GitHub dependency alerts do not cover every transitive/native dependency or establish complete license/CVE compliance.
- A private fallback security/conduct/privacy contact and any support commitment require actual maintainer information. No address or SLA is assumed.

Read the [release checklist](../RELEASE_CHECKLIST.md) and [standards assessment](STANDARDS_ASSESSMENT.md) for the full acceptance matrix. A no-warranty notice explains terms and risk; it does not prevent bugs or make destructive operations reversible.
