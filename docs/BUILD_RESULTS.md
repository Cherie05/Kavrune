# Local validation — 14–17 October 2026

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

## Alpine hybrid ISO compatibility — 17 October 2026

A physical-trial preparation exposed an image-inspection bug before any drive writing: Alpine's first GPT carrier begins at sector zero, preceding the GPT usable range while remaining inside the file. The inspector now recognizes this bounded hybrid convention after validating ISO/boot structures and the whole-image MBR carrier. Ordinary GPT checks remain strict.

- The user's completed Alpine Extended 3.24.2 x86_64 ISO is 1,456,766,976 bytes. SHA-256 matches both its downloaded sidecar and the checksum fetched from Alpine's official HTTPS mirror: `a22115a46a1bcd876234a6cd43797fe19f4acfc4512fb41ea64e16cc2ffb4d90`.
- The updated CLI identifies that exact image as `Hybrid ISO`, `bootable: true`, `raw_writable: true`. This identifies embedded boot metadata; it is not a boot test.
- A complete write and mandatory SHA-256 readback simulation of that ISO passed in a new temporary regular file. The temporary copy was removed; no block device was accessed.
- An original synthetic hybrid fixture adds 22 regression assertions for accepted layouts, malformed/truncated ISO structures, incorrect MBR carriers, GPT bounds/checksums and retained Windows restrictions. Core: **138 checks**; storage: **122 checks**; total: **260 checks**. All five CTest suites passed in standard and AddressSanitizer/UndefinedBehaviorSanitizer builds.

The image-inspection fix does not establish a physical USB, Alpine boot, Polkit or firmware qualification result. No physical USB was written during this investigation. Existing app processes must be closed and reopened to load the rebuilt binary.

## Broader image compatibility — 17 October 2026

The inspector was extended across the [documented image profiles](IMAGE_COMPATIBILITY.md), with bounds and write restrictions retained. The final local validation recorded:

- Core regression suite: **194 checks passed**; storage regression suite: **122 checks passed**; total: **316 checks**.
- All five standard CTest checks passed in 1.03 seconds. All five AddressSanitizer/UndefinedBehaviorSanitizer CTest checks passed in 2.69 seconds.
- Regression fixtures cover BIOS/UEFI default and section catalogs, EFI zero/one-sector load conventions, section-entry extensions, alternate MBR/GPT carrier types, unused GPT slots, enhanced ISO descriptors, repeated ISO terminators, ISO/UDF bridges and retained malformed-image/Windows restrictions. Independent code review identified and corrected repeated-terminator and boot-entry-extension handling.
- CLI JSON success and typed issue outputs were checked. Core identity, checksum and write-permission tests cover the logic used by checksum-only selections. Interactive chooser behavior and the complete fallback UX were **not automated or visually verified** for this update.
- The rebuilt desktop startup smoke test exited successfully with no standard-error output. This checks startup, not interactive image selection or USB operations.
- The completed Alpine Extended ISO above was recognized and copied into a new temporary regular file. Its full 1,456,766,976 bytes passed mandatory SHA-256 readback verification against `a22115a46a1bcd876234a6cd43797fe19f4acfc4512fb41ea64e16cc2ffb4d90`, also matched to the official publisher checksum as recorded above.
- `android-x86-6.0-r1.iso`, 566,231,040 bytes, was recognized and passed a full temporary-file write and SHA-256 readback comparison. An official publisher checksum was **not fetched** for that file; this result establishes copy equality, not download authenticity.
- Both temporary image copies were deleted. No physical drives were accessed or written by these simulations.
- `scripts/package.sh` rebuilt the Debian package and direct dependency inventory after all five tests passed. Extraction without installation confirmed matching executable build IDs, no setuid or RPATH/RUNPATH entries, current guides and notices, recognition of both exact ISOs, and a passing CLI safe self-test with `physical_usb_tested: false`.

These results establish structural recognition and file-copy verification for those exact files, plus the extracted package checks described above. They do not establish physical USB reliability, successful Alpine/Android-x86 installation or booting, UEFI/Secure Boot qualification, or general support for every ISO. Windows extraction, UDF-only/optical-only USB creation and compressed-image streaming remain unimplemented.

## Drive selection and task guidance update

A reported mismatch between the visible USB dropdown and the selected-target state was reproduced in the desktop model. The UI now uses a separate **Choose a USB drive…** placeholder, replaces discovery results together, and synchronizes the same physical selection across Create, Format, Backup and Check USB. A missing or changed device clears the target rather than selecting another drive.

Create USB now shows preparation and boot instructions before image selection. Formatting uses plain-language file-storage choices and validates names/options before confirmation. Backup rejects existing filenames before confirmation and checks available space before unmounting. Read-only operations explain reopening USB volumes afterward, and checksum results identify the algorithm actually calculated.

The storage integration's UDisks method signatures were reviewed without changing devices; the review found no interface mismatch in the advertised operation calls. Read-only `CanFormat` queries reported available utilities for `vfat`, `exfat`, and `ext4`. This establishes advertised utility availability, not successful physical formatting, authorization or recovery behavior.

The completed sanitizer build validated:

- **194 core**, **122 storage**, **329 real GTK selection**, and **55 actual desktop-workflow** assertions: **700 checks** in total. The GTK selection suite constructs no window; the workflow suite constructs the real desktop window without presenting it. Neither starts discovery or opens a physical drive.
- Selection cases cover a sole drive, explicit choice, all four tool selectors, repeated refresh, disconnect/replacement, and placeholder handling. Actual desktop controls verify matching drive details/review, checksum-mismatch blocking, too-small targets, busy-state gating, source reset, and format-name limits through live GTK signals.
- All **seven AddressSanitizer/UndefinedBehaviorSanitizer CTest suites passed in 3.07 seconds**. Leak detection is disabled only for the two GTK suites because GTK retains process-wide caches; core and storage leak detection remains enabled.
- `scripts/package.sh` passed all **seven standard CTest suites in 2.16 seconds** and generated the updated Debian package and direct dependency SPDX inventory.
- Extraction without system installation confirmed matching desktop/CLI build IDs, no RPATH/RUNPATH or setuid files, and inclusion of notices and [USER_TEST_CHECKLIST.md](USER_TEST_CHECKLIST.md). The extracted CLI safe self-test passed in JSON mode with `physical_usb_tested: false`.
- A private X11/Xvfb and D-Bus session verified all seven navigation pages, the visible preparation/boot instructions, and the USB-choice placeholder. Default 1120×900 and compact 800×600 layouts were checked for wrapping and sidebar behavior; [preview.png](preview.png) and [preview-small.png](preview-small.png) were updated.
- Visual QA reported no GTK warnings or application failures. Standard error contained only expected headless DRI3/libEGL acceleration warnings. The file chooser and live USB selection were not exercised in this visual run; the hidden desktop tests above cover selection-control logic. No storage operation or confirmation was invoked, and the isolated QA processes were stopped afterward.

Earlier counts elsewhere in this document apply to their respective revisions. No physical USB writing, formatting, authorization prompt or firmware boot has been qualified by this review. Use [USER_TEST_CHECKLIST.md](USER_TEST_CHECKLIST.md) to record a separate spare-USB trial.

## Published revision CI

[GitHub Actions run 38540398428](https://github.com/Cherie05/Kavrune/actions/runs/38540398428) completed successfully for commit `29b3875073378c28a4ff0c6421b825f066e0fe7c`, created on 14 October 2026 at 14:40 IST. This is evidence for that published revision. Subsequent beginner UI, safe self-test, storage-protection and image-compatibility changes require their own CI run after publication; the earlier green run does not validate those changes.

Reproduce release/file tests with `scripts/build.sh`, optional synthetic boot with `scripts/test-boot.sh build/kavrune-cli`, and packaging with `scripts/package.sh`. For a sanitizer build, configure a separate build directory with `-DKAVRUNE_SANITIZERS=ON`. Refer to `VALIDATION.md` for the outstanding release gates.
