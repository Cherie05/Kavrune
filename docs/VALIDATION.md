# Validation and release gates

## Automated checks in this checkout

`scripts/build.sh` compiles the desktop and CLI, then runs engine regression tests, storage safety/identity tests, CLI help, the safe self-test and a CLI file simulation. All automated write tests use disposable regular files. `kavrune --smoke-test` checks its own UI initialization with a display, even if another Kavrune instance is open, and performs no destructive operation.

`scripts/test-boot.sh build/kavrune-cli` assembles an original 512-byte x86 boot sector into a 4 MiB synthetic image, writes/verifies a regular-file copy through the CLI, and boots that copy with QEMU/SeaBIOS. The guest emits an exact success marker and exits via a bounded debug device. This demonstrates the file-to-boot pipeline; it does not qualify any distribution or UEFI firmware.

The CI workflow builds on Ubuntu 24.04, runs tests and packages. Its GUI smoke test uses a virtual display. A second build enables AddressSanitizer and UndefinedBehaviorSanitizer. [The published baseline passed GitHub CI](https://github.com/Cherie05/Kavrune/actions/runs/38540398428) on 14 October 2026. That result applies to its recorded commit; changes made afterward require a new hosted run.

`kavrune-cli self-test` and the desktop's **Try a safe test** use the shared writer to copy and verify a generated sample in a private temporary folder. They need no image download, USB drive or authorization. This proves the temporary-file engine path works; it does not validate physical block-device access or firmware booting.

## Required before a production release

- Real disposable USB: USB sticks, external SSDs, SD readers, serial-less devices, 512/4096-byte logical sectors and adapters with misleading removable flags.
- Authorization: approve/deny/cancel Polkit; no agent; unavailable UDisks; user without permission; headless CLI agent.
- Race/error injection: unplug during discovery, authentication, unmount, write, synchronization, verification, format and rescan; reconnect another device using the same kernel path; busy mounts and swap; source changes.
- Storage protection: plain ext4 root, separate /home, LUKS/LVM, MD RAID, multi-device Btrfs, overlay/container root, nonstandard mounts and bind mounts; prove rejection when required ancestry is unknown.
- Formatting: GPT/MBR and FAT32/exFAT/ext4; missing filesystem tools; rejected labels; errors between table creation and filesystem creation; automount races.
- Raw preparation: stale secondary GPT/filesystem signatures beyond an incoming image's length; signature cleanup strategy and target-tail confidentiality. Raw writes are not secure erasure.
- Boot: legitimate Ubuntu/Debian/Fedora/Arch hybrid images on BIOS SeaBIOS and UEFI OVMF/QEMU; actual hardware Secure Boot. Record image SHA-256, firmware version, target hardware, logs and result. Do not ship copyrighted test ISOs in the repository.
- Packaging: build/install/uninstall clean VMs on each supported distribution; desktop launcher and icon; runtime dependency minima; package reproducibility; no developer paths in installed runtime search paths. AppStream now links to the project repository; validate the metadata as part of each release.
- Legal/privacy: offline Terms, Privacy, Disclaimer and MIT License must match the packaged files; verify Help/About access, searching, wrapping and keyboard use. Confirm actual data handling and retention against the notice, publish a working private contact, and obtain the legal review appropriate to the intended distribution.
- Independent parser/storage review, dependency vulnerabilities/license review, trademark clearance, complete SBOM, accessibility with Orca/keyboard/high-contrast/scaled text, translations and crash/recovery review.

Physical media tests are not executed on a developer's attached drives by the automated suite. Loop-device and QEMU tests should run in a dedicated disposable VM before real-device validation. The preview must not be presented as satisfying these release gates until results are recorded.

`scripts/package.sh` now extracts the new package and runs `scripts/check-package.py --self-test` before accepting it. This validates packaged/embedded notices, guides, metadata, permissions, ELF hardening and the extracted CLI's disposable-file test without system installation or physical-drive access. It does not replace clean-VM installation, real storage, independent security, privacy-law or firmware qualification. See [PRODUCTION_AUDIT.md](PRODUCTION_AUDIT.md) for the current assessment.
