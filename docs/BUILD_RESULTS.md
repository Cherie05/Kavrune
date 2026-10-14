# Local validation — 14 October 2026

Build host: Ubuntu 26.04.1 LTS x86_64, GCC 15.2, GTK 4.22.4, libadwaita 1.9.1, GLib/GIO 2.88.0, OpenSSL 3.5.5. Build dependencies were extracted under `.deps`; no system package installation or physical-device writing occurred.

Results obtained in this workspace:

- Desktop and CLI release builds passed with compiler warnings enabled.
- Core regression suite: 93 assertions passed, using temporary regular files.
- Storage regression suite: 108 assertions passed, without opening storage devices.
- CTest: core safety, storage safety, CLI help, and complete CLI file simulation all passed.
- AddressSanitizer plus UndefinedBehaviorSanitizer: all four test suites passed in a separate debug build.
- Live UDisks enumeration: the attached internal NVMe device was classified as protected system storage; no eligible USB was attached.
- GTK UI smoke test: successful initialization and exit with no storage changes. All six pages and a 900×650 window were visually checked; Ctrl+1–6 navigation validated.
- Original synthetic BIOS image: CLI wrote and verified a 4 MiB regular-file image; QEMU/SeaBIOS booted the result, emitted `KAVRUNE_BOOT_OK`, and exited with expected debug status 33. The backing image remained unchanged after the guest ran.
- Debian packaging succeeded, including dynamically linked desktop/CLI binaries, desktop entry, original icon, AppStream XML and documentation. A direct dependency SPDX inventory is generated separately.
- The final Debian package was extracted to a temporary directory; its installed desktop smoke test and CLI simulation passed without system installation. Installed executables have no RPATH/RUNPATH referring to the developer workspace.

These results do not validate real USB operations, Linux installer boots, UEFI/Secure Boot, Polkit dialogs, format-tool execution, power loss or unplug races. They do not certify enterprise readiness or legal clearance. The CI workflow is prepared but has not run remotely in this checkout. AppStream validation reports a missing real project homepage; this remains a publishing requirement.

Reproduce release/file tests with `scripts/build.sh`, optional synthetic boot with `scripts/test-boot.sh build/kavrune-cli`, and packaging with `scripts/package.sh`. For a sanitizer build, configure a separate build directory with `-DKAVRUNE_SANITIZERS=ON`. Refer to `VALIDATION.md` for the outstanding release gates.
