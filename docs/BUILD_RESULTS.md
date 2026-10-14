# Local validation — 14 October 2026

Build host: Ubuntu 26.04.1 LTS x86_64, GCC 15.2, GTK 4.22.4, libadwaita 1.9.1, GLib/GIO 2.88.0, OpenSSL 3.5.5. Build dependencies were extracted under `.deps`; no system package installation or physical-device writing occurred.

## Baseline before the beginner workflow update

The following results were previously obtained in this workspace. They describe the earlier six-page desktop and do not by themselves validate the subsequent UI changes:

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

These results do not validate real USB operations, Linux installer boots, UEFI/Secure Boot, Polkit dialogs, format-tool execution, power loss or unplug races. They do not certify enterprise readiness or legal clearance.

## Beginner workflow update

The initial beginner-workflow build and CTest run on 14 October 2026 at 14:52 IST passed:

- Desktop and CLI builds completed.
- Core regression suite: 103 assertions passed, including the shared self-test's success, cancellation and cleanup paths.
- Storage regression suite: 122 checks passed, including account-home protection and symlink home destinations; no storage devices were opened.
- All five registered CTest checks passed: core safety, storage safety, CLI help, CLI safe self-test and CLI file simulation.
- The safe self-test wrote and verified 8,388,645 bytes in temporary regular files. SHA-256 was `7e924d8131236a13f61eb910089790a246d21aefcf7117c6f96565af50bf91be`; JSON explicitly reported `physical_usb_tested: false`.

The safe test checks the same write and SHA-256 readback engine used by USB creation. It requests no block device or Polkit authorization. Its successful result does not establish physical USB reliability or boot compatibility.

AppStream metadata now includes the project's GitHub homepage and issue tracker. `appstreamcli validate --no-net data/org.kavrune.Kavrune.metainfo.xml` passed; its only pedantic hint concerns the existing mixed-case application ID. The previous missing-homepage finding is resolved. This validates metadata syntax and requirements, not network reachability or release readiness.

## Revised build after source-image protection

The revised code was built and checked on 14 October 2026:

- Desktop and CLI builds completed with no compiler warnings.
- Core regression suite: **116 checks passed**, including source change-time checks and selected-image SHA-256 pinning.
- Storage regression suite: **122 checks passed**. Combined core/storage coverage is **238 checks** using temporary files and simulated storage metadata.
- All five standard CTest checks passed in 0.78 seconds.
- All five AddressSanitizer plus UndefinedBehaviorSanitizer CTest checks passed in 1.90 seconds, within the configured 90-second per-test limit. No sanitizer errors were reported.
- The synthetic 4 MiB BIOS image was written and verified again, then booted under QEMU/SeaBIOS. It emitted `KAVRUNE_BOOT_OK`, exited with expected status 33, and left the backing image unchanged. This is not a real installer or physical USB boot test.
- The desktop smoke test opened its own application instance and exited successfully in 2.491 seconds, without GTK warnings or storage changes.
- All seven desktop pages were visually checked, and **Try a safe test** completed in the GUI with **Safe test passed**. No physical drive was opened or changed by the test.
- Layouts at 800×600 and 420×480 were checked. The creation steps, create button, result and expandable technical details remained reachable by scrolling. Compact navigation closed when selecting the current page; enlarging the window restored the sidebar. The GUI QA process produced no standard-error output.
- Updated normal and narrow-window screenshots are saved as [preview.png](preview.png) and [preview-small.png](preview-small.png).
- `scripts/package.sh` passed all five CTest checks and produced the updated Debian package and direct dependency SPDX inventory.
- The package was extracted without system installation. Its executable code/data sections and build IDs matched the current build. No RPATH/RUNPATH entries or setuid files were found. The desktop launcher, icon, AppStream metadata, notices and beginner guides were included.
- The extracted package's CLI safe test passed in JSON mode, writing and verifying 8,388,645 bytes with SHA-256 `7e924d8131236a13f61eb910089790a246d21aefcf7117c6f96565af50bf91be` and reporting `physical_usb_tested: false`.
- Local Markdown links resolved and the repository publication check passed.

These results validate the tested file-based engine, protection logic, desktop workflow and extracted-package contents. GUI file-chooser, checksum-mismatch and invalid-file automation was not confirmed; those user interactions still need manual validation. No package was installed system-wide and no actual drive was written. Real USB operations and the release gates in [VALIDATION.md](VALIDATION.md) remain pending.

## Published revision CI

[GitHub Actions run 38540398428](https://github.com/Cherie05/Kavrune/actions/runs/38540398428) completed successfully for commit `29b3875073378c28a4ff0c6421b825f066e0fe7c`, created on 14 October 2026 at 14:40 IST. This is evidence for that published revision. The subsequent beginner UI, safe self-test and storage-protection changes require their own CI run after publication; the earlier green run does not validate those changes.

Reproduce release/file tests with `scripts/build.sh`, optional synthetic boot with `scripts/test-boot.sh build/kavrune-cli`, and packaging with `scripts/package.sh`. For a sanitizer build, configure a separate build directory with `-DKAVRUNE_SANITIZERS=ON`. Refer to `VALIDATION.md` for the outstanding release gates.
