# Kavrune release checklist

Release status remains **development preview** until every applicable release gate has recorded acceptance. This is a checklist, not evidence that the entries passed. See [STANDARDS_ASSESSMENT.md](docs/STANDARDS_ASSESSMENT.md) for the current assessment.

- [ ] Review exact source revision, intended version, changelog, feature scope, target distro/architecture and maintainer/support identity.
- [ ] Read [PRODUCT_STANDARDS.md](PRODUCT_STANDARDS.md); run source hygiene and review content, errors, privacy, data retention and all applicable policies.
- [ ] Build desktop/CLI and all core/storage/UI suites; repeat with sanitizers. Investigate failures and skips, and record exact commands/results.
- [ ] Check actual artifact offline operation and provisional local smoke budgets. Separately meet baseline hardware startup/idle/interaction and memory targets.
- [ ] Verify UI keyboard, labels/focus, Orca, text scaling, compact/default, light/dark, contrast and reduced motion; record supported language limits.
- [ ] Complete disposable storage, interruption, authorization, format/backup/restore and system-drive protection matrix. Validate recovery instructions and stale-tail policy.
- [ ] Record intended Linux image checksums and BIOS/UEFI/Secure Boot boot results on supported hardware. Do not bundle OS test ISOs.
- [ ] Run package extraction, embedded-notice equality, hardening, metadata, offline and payload lifecycle gates. Build on the oldest claimed distro.
- [ ] Separately test real package-manager clean install/reinstall/upgrade/remove and offline installation with dependencies present in disposable VMs. Verify desktop launcher and user-data retention.
- [ ] Audit actual direct/transitive library versions, advisories and license obligations; produce a complete SBOM and retained required notices/source/relink access.
- [ ] Obtain independent parser/storage security review, appropriate legal/publisher/name review, and a working private security/privacy channel. Do not claim a contact or SLA that has not been set up.
- [ ] Build reviewed artifacts with source fingerprint/provenance and hashes. Authenticate releases using a maintainer-controlled signing identity and independently published fingerprint/trusted repository. Plain SHA-256 and unsigned manifests provide integrity only; no signing key is created or silently selected by Kavrune.
- [ ] Document manual update/downgrade/recovery behavior, supported versions, retention and security maintenance. No automatic updater or database migrations exist in this version.
- [ ] Check exact published revision in hosted CI; local checks do not verify remote settings or older CI results. Publish as a pre-release until qualification is complete.

## Install, update and remove contract

After reviewing a complete artifact set, a maintainer can run `sh scripts/sign-release.sh FULL_EXISTING_SIGNING_KEY_FINGERPRINT` with an existing publisher-controlled GPG signing key. The script verifies checksums, creates and verifies a detached signature, and refuses to overwrite one. Publish the public-key fingerprint through an independently trusted channel. Packaging never creates a key or invokes signing automatically. Verification is `gpg --verify SHA256SUMS.asc SHA256SUMS` followed by `sha256sum --check SHA256SUMS`; signature validity alone does not establish publisher trust.

The package installs application-owned executables, desktop metadata, an icon and documentation under `/usr`. It adds no private daemon, Polkit bypass, setuid helper, autostart or scheduled task. The current release uses system UDisks2/Polkit. System packages supply runtime libraries; a downloaded DEB alone is not an offline dependency bundle.

Close Kavrune after any operation has safely finished before an upgrade/removal. Use the distribution's package manager to install a trusted compatible package and reopen it; there is no in-app updater. Never automatically downgrade a package or restore an old job. User-selected ISOs/backups and XDG diagnostic files are outside the installed payload and retained on uninstall. Clear diagnostics through Activity/CLI first if desired; deleting a user backup requires the user's own separate action.

Extraction/overlay/removal checks demonstrate payload behavior in a temporary directory only. They do not exercise dpkg dependency resolution, desktop caches, authentication, maintainer scripts, or a clean target distribution. Those remain required disposable-VM tests.
