# Changelog

## Unreleased

- Make beta/development status and possible data loss, broken media and device/system damage explicit in project notices while preserving the MIT license and applicable-law qualifications. Add a user installation, verification, safe-test, update and removal guide; keep PPA setup deferred.
- Align contributor publishing instructions with protected-branch PR requirements and extend ignored local credential/crash files.

Future work is tracked in [ROADMAP.md](docs/ROADMAP.md). The current downloadable preview is recorded below.

## 0.1.0 Preview 1 — 26 October 2026

Published as [v0.1.0-preview.1](https://github.com/Cherie05/Kavrune/releases/tag/v0.1.0-preview.1), with package version `0.1.0`. The release contains the Ubuntu 24.04 amd64 CI build, exact-commit source archive, scoped dependency inventory, offline report and unsigned integrity/provenance records.

- Add release download/install guidance and direct links to the official repository's enabled private vulnerability reporting channel.
- Keep the application alive until background drive discovery returns, including after closing the last window, to prevent shutdown from racing storage cleanup. Test successful and failed discovery against the actual application loop with synthetic metadata.
- Apply the complete desktop checklist through permanent product requirements/standards, architecture, privacy/legal inventory, test and release plans, and an assessment of all 25 sections and 24 release gates.
- Add bounded UTC/severity diagnostic history, safe reload/export/confirmed clearing in Activity and CLI, current-version reports, Unicode/control sanitization and interprocess safety tests. Synchronize privacy notices with size retention and user data controls.
- Improve persistent/accessibility labels, native reduced motion and best-effort active-job suspend/logout inhibition. Embed third-party notices and support information beside the legal documents.
- Verify normal and extracted builds with process-local internet socket denial, record provisional smoke timing/RSS, check disposable payload lifecycle and metadata/version consistency, and generate unsigned source/artifact integrity manifests. Provide an optional manual signing command for an existing publisher key; no keys or signatures are created automatically.
- Add Terms and Privacy notices and an offline Legal & privacy reader in Help/About with document switching, text selection, search, keyboard shortcuts and compact layouts; keep MIT rights and preview/warranty limits clear.
- Prevent optional event logging from blocking on substituted FIFOs or a competing log writer, and reject backup destinations moved or replaced during saving before reporting success.
- Verify new Debian packages by extraction without installation, including current guides/notices, embedded legal documents, executable hardening and the CLI safe self-test; add desktop metadata/package checks to CI and record the production audit.
- Show five creation stages and animate blocking save/finalization steps with a spinner and elapsed-time updates. Keep overall progress below 100% until the operation finishes, retain the selected image/USB review while busy, and explain safe cancellation while buffered data is being saved.
- Fix USB dropdowns showing a drive while the selected-target state remained empty. Add an explicit choice placeholder, synchronize drive selection across all four storage tools, preserve the same device across refreshes, and clear disconnected targets.
- Show preparation and computer-boot instructions on Create USB before image selection, including that formatting first is unnecessary. Simplify Format USB around use and USB name, place partition options under Advanced settings, and explain invalid options before erase confirmation.
- Reject existing backup names before confirmation, check available backup space before unmounting, explain reopening volumes after backups/read scans, identify calculated checksum algorithms in results/copy actions, and keep confirmation callbacks tied to the live desktop.
- Add real GTK selection and desktop-workflow regression tests plus a [spare-USB test checklist](docs/USER_TEST_CHECKLIST.md). Physical writing, authorization and boot qualification remain separate release requirements.
- Recognize bootable hybrid ISOs with a sector-zero GPT carrier, including Alpine Extended, while retaining GPT checksums, image bounds and strict ordinary disk-image validation.
- Broaden hybrid image inspection to BIOS/UEFI-only boot catalog sections, EFI zero/one-sector load conventions, generator-selected MBR/GPT carrier types, enhanced ISO descriptors and ISO/UDF bridges.
- Explain unfinished, compressed, virtual-disk, malformed and unsupported images with specific next steps; allow complete unsupported files in Check download without granting USB-write permission. Document the [compatibility profiles](docs/IMAGE_COMPATIBILITY.md) and inspection limits.
- Adopt Kavrune as the project, desktop application, CLI and package name.
- Provide an original C++20 GTK4/libadwaita desktop and shared CLI engine.
- Organize the desktop into seven plain-language tasks with numbered creation steps, clear drive selection, progress and an in-app beginner guide.
- Add **Try a safe test** and CLI `self-test [--json]` to write and verify temporary files without changing a USB.
- Write supported raw/hybrid images with SHA-256 readback verification.
- Discover external drives through UDisks2 and validate identity and protected storage ancestry before destructive operations.
- Protect account home storage, including homes reached through symlinks, when determining which drives are safe to offer.
- Pin the selected image's file identity, change timestamp and SHA-256 so changed source metadata or content cannot be reported as a successful write of the selected image.
- Offer GPT/MBR formatting, raw backups, image checksums and read-only drive scans.
- Include safe regular-file regression tests, sanitizer configuration, optional synthetic BIOS boot testing and Debian packaging.
- Add MIT licensing, third-party notices, security reporting guidance, storage disclaimer, contribution and conduct policies, GitHub templates and repository ignore rules.
- Describe Kavrune independently and make no-warranty and damage-liability notices explicit in the README, NOTICE and disclaimer, subject to applicable law and the MIT License.

This is a published development preview, not a stable release. Physical USB and production qualification remain pending. Windows installer preparation, persistence and other later features are tracked in [ROADMAP.md](docs/ROADMAP.md).
