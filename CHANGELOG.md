# Changelog

## 0.1.0 — Unreleased development preview

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
