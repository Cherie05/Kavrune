# Validation and release gates

## Automated checks in this checkout

`scripts/build.sh` compiles the desktop and CLI, then runs engine regression tests, storage safety/identity tests, CLI help and a CLI file simulation. All automated write tests use disposable regular files. `kavrune --smoke-test` checks UI initialization with a display and performs no destructive operation.

`scripts/test-boot.sh build/kavrune-cli` assembles an original 512-byte x86 boot sector into a 4 MiB synthetic image, writes/verifies a regular-file copy through the CLI, and boots that copy with QEMU/SeaBIOS. The guest emits an exact success marker and exits via a bounded debug device. This demonstrates the file-to-boot pipeline; it does not qualify any distribution or UEFI firmware.

The CI workflow builds on Ubuntu 24.04, runs tests and packages. Its GUI smoke test uses a virtual display. A second build enables AddressSanitizer and UndefinedBehaviorSanitizer. This workflow must actually run in the connected repository before claiming CI success.

## Required before a production release

- Real disposable USB: USB sticks, external SSDs, SD readers, serial-less devices, 512/4096-byte logical sectors and adapters with misleading removable flags.
- Authorization: approve/deny/cancel Polkit; no agent; unavailable UDisks; user without permission; headless CLI agent.
- Race/error injection: unplug during discovery, authentication, unmount, write, synchronization, verification, format and rescan; reconnect another device using the same kernel path; busy mounts and swap; source changes.
- Storage protection: plain ext4 root, separate /home, LUKS/LVM, MD RAID, multi-device Btrfs, overlay/container root, nonstandard mounts and bind mounts; prove rejection when required ancestry is unknown.
- Formatting: GPT/MBR and FAT32/exFAT/ext4; missing filesystem tools; rejected labels; errors between table creation and filesystem creation; automount races.
- Raw preparation: stale secondary GPT/filesystem signatures beyond an incoming image's length; signature cleanup strategy and target-tail confidentiality. Raw writes are not secure erasure.
- Boot: legitimate Ubuntu/Debian/Fedora/Arch hybrid images on BIOS SeaBIOS and UEFI OVMF/QEMU; actual hardware Secure Boot. Record image SHA-256, firmware version, target hardware, logs and result. Do not ship copyrighted test ISOs in the repository.
- Packaging: build/install/uninstall clean VMs on each supported distribution; desktop launcher and icon; runtime dependency minima; package reproducibility; no developer paths in installed runtime search paths. Establish a real project homepage before AppStream distribution (metadata currently reports the missing-homepage warning).
- Independent parser/storage review, dependency vulnerabilities/license review, trademark clearance, complete SBOM, accessibility with Orca/keyboard/high-contrast/scaled text, translations and crash/recovery review.

Physical media tests are not executed on a developer's attached drives by the automated suite. Loop-device and QEMU tests should run in a dedicated disposable VM before real-device validation. The preview must not be presented as satisfying these release gates until results are recorded.
