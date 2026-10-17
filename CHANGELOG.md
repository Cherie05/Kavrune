# Changelog

## 0.1.0 — Unreleased development preview

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

This entry describes preview functionality and repository preparation, not a published stable release. Physical USB and production qualification remain pending. Windows installer preparation, persistence and other later features are tracked in [ROADMAP.md](docs/ROADMAP.md).
