# Local validation — 14–26 October 2026

Build host: Ubuntu 26.04.1 LTS x86_64, GCC 15.2, GTK 4.22.4, libadwaita 1.9.1, GLib/GIO 2.88.0, OpenSSL 3.5.5. Build dependencies were extracted under `.deps` without system package installation. Agent-run write tests used temporary files; a later user-initiated physical USB trial is recorded separately below.

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

## User-initiated Alpine USB trial — 17 October 2026

The application log records a user-initiated trial on revision `40f7957`: **Creating your USB** at 23:18:37 IST, followed by **USB written and verified** at 23:21:50 IST. The selected image was Alpine Extended 3.24.2 x86_64 and the target was a Cruzer Blade reported as 29.2 GiB. The recorded written-image SHA-256 was `a22115a46a1bcd876234a6cd43797fe19f4acfc4512fb41ea64e16cc2ffb4d90`, matching the Alpine image recorded above.

This is one observed application-log success from the user's physical trial, not an agent-initiated destructive test or an independently witnessed boot result. No result is available for starting a computer from that USB. It does not qualify other USB hardware, formatting, all authorization paths, BIOS/UEFI/Secure Boot, or enterprise deployment.

## Save-stage progress update

Following the user's trial, the desktop now distinguishes checking the image, writing, saving buffered data, verification, and finalization. Saving and finalization use an animated indicator and elapsed-time updates instead of displaying a completed operation while a blocking storage call is still running. Overall 100% is reserved for a successful return after verification and device rescan. Cancellation explains that buffered data must be saved before stopping; failures and cancellation stop the animation without showing successful completion.

The update also keeps the chosen image and USB visible during the operation, marks the image as in progress, and rejects stale progress callbacks from an earlier operation. It changes desktop progress reporting, not the I/O engine. The physical trial above used the earlier build; it does not validate the new progress display.

The rebuilt update passed all seven standard CTest suites in **1.65 seconds** and all seven AddressSanitizer/UndefinedBehaviorSanitizer suites in **3.65 seconds**, with no new compiler warnings. Coverage is **194 core + 122 storage + 329 GTK selection + 80 actual desktop-workflow assertions = 725 checks**. The desktop checks include progress/finalization behavior without physical USB operations. Leak detection remains disabled only for the two GTK suites because of process-wide GTK caches.

CTest now locates and uses a private X11/Xvfb session when the wrapper is available. A separate run of both GTK suites from a shell without the local development environment passed in **0.73 seconds**, checking that the test runner does not depend on sourcing the developer's environment first.

`scripts/package.sh` passed all seven standard suites in **1.63 seconds** and rebuilt the Debian package and direct dependency SPDX inventory. Extraction without installation confirmed current executable build IDs and guides/notices, no RPATH/RUNPATH or privileged files, and a passing disposable-file CLI self-test with `physical_usb_tested: false`.

A temporary preview harness rendered the saving stage on a private X11/Xvfb and D-Bus session with synthetic image/USB details, without starting discovery or a storage operation. The [saving-stage preview](progress-saving.png) shows the spinner, activity bar, step 3 of 5, elapsed time and retained selection review. The preview exited successfully and its private processes closed; no GTK warnings or criticals were reported, only headless graphics/session diagnostics.

## Production audit and legal/privacy update — 18 October 2026

The [production audit](PRODUCTION_AUDIT.md) records scope, addressed findings and remaining release gates. Terms and Privacy were added; all four legal documents are packaged and embedded in an offline reader available from How to use and About. The notices preserve the MIT License and explain actual local processing and persistent log retention without claiming legal certification.

- Four concrete cases were fixed: a substituted FIFO blocking log open, a contended log lock blocking the UI, and a backup file or its parent folder being replaced during synchronization while the original selected path could incorrectly be reported as successful. Fourteen new core assertions reproduce these cases and verify recovery/replacement-safe cleanup.
- The completed build passed all **seven standard CTest suites in 1.79 seconds** and all **seven AddressSanitizer/UndefinedBehaviorSanitizer suites in 4.08 seconds**. Coverage is **208 core + 122 storage + 329 GTK selection + 100 desktop-workflow assertions = 759 checks**. No new compiler warnings were reported. Leak detection is disabled only for GTK's process-wide caches; core and storage leak checks remain enabled.
- Twenty new hidden desktop checks cover the legal resources and reader: complete notice text, document switching, case-insensitive search, next/previous and absent matches, clearing search, and closing the reader without changing an active operation's selection, ID or cancellation state.
- Private X11/Xvfb and D-Bus visual checks covered the reader at 700×600 and 420×600 in light and dark themes, plus the actual About-to-reader link. No discovery or storage operation was invoked. The [wide](legal-privacy.png) and [compact](legal-privacy-small.png) captures show the dark-theme reader. No application GTK warning or critical was reported; private-session graphics/portal diagnostics remain expected.
- AppStream validation without network access and desktop-entry validation passed. The AppStream mixed-case application-ID pedantic hint remains; it is not a validation failure. Shell syntax, Python compilation, local documentation links and repository publication checks passed.
- `scripts/package.sh` rebuilt the Debian package and direct runtime SPDX inventory, then extracted the new archive without installing it. The new automatic gate verified current notices/guides/metadata, byte-identical embedded legal documents, safe file permissions, no setuid/setgid or RPATH/RUNPATH entries, and PIE/RELRO/BIND_NOW/non-executable-stack hardening. Payload archive owner/group IDs were separately confirmed as root/root. The extracted CLI's disposable-file JSON self-test passed with `physical_usb_tested: false`.
- A disposable repacked archive with `PRIVACY.md` deliberately removed was rejected by the gate. The original artifact remained unchanged. Package validation checks only archives created in the current run, avoiding failures caused by unrelated older artifacts in `dist/`.
- The synthetic 4 MiB BIOS test passed again using local QEMU 10.2.1/SeaBIOS. The CLI wrote and verified a temporary regular file, the guest emitted `KAVRUNE_BOOT_OK` with expected exit status 33, and the backing file remained unchanged. This is not a real Linux distribution, UEFI or physical USB boot result.

No system package installation or physical USB operation was performed for this audit. New hosted CI metadata/package checks are configured; a successful hosted run for the final audit revision has not been observed here. The app remains a development preview with hardware, firmware, distribution, accessibility, independent security, complete dependency/SBOM and operational release qualification outstanding.

## Offline desktop standards implementation — 26 October 2026

The entire supplied desktop checklist was read in full and adapted to the application. [STANDARDS_ASSESSMENT.md](STANDARDS_ASSESSMENT.md) maps all **25 sections and 24 release criteria**, including why server/accounts/sync/billing/webview/database requirements are inapplicable and which production gates still require external evidence. Root product requirements, standards, architecture, privacy/legal inventory, test plan and release checklist are shipped as documents, with AGENTS rules for future source changes.

Implemented improvements include bounded persistent diagnostics (current/previous generations up to 256 KiB each, 8 KiB records), safe UTF-8/control sanitization, UTC/severity, stable interprocess locking, no-clobber private export and confirmed clearing. Activity adds retained-history reload, export and clearing with a bounded live buffer. Diagnostics export is asynchronous and exclusive with new jobs. Accessible field labels/relationships, live reduced-motion behavior, best-effort suspend/logout inhibition and generated version metadata were added. Six offline reader documents now include dependency notices and support. The privacy notice was updated to actual retention, deletion, command logging and sharing behavior.

- Final code validation passed **all eight normal CTest suites in 4.84 seconds** and **all eight AddressSanitizer/UndefinedBehaviorSanitizer suites in 5.09 seconds**. Coverage is **248 core + 122 storage + 329 GTK selection + 139 actual desktop-workflow assertions = 838 checks**, plus CLI integration/offline fixtures. No compiler warnings were reported in those build logs.
- New core cases cover bounded rotation, a 64 MiB sparse oversized legacy log, record limits, UTC/severity, ordinary Unicode and invalid/control text, private exclusive exports, unsafe active/rotated/lock links, actual child-process locking, contention and clear/append races. CLI integration checks explicit clear confirmation, no-clobber export and Unicode filenames with spaces.
- Actual GTK checks use the test accessibility backend to inspect native labels/relations. They cover retained history, confirmation cancellation/clearing, background export success/failure, export/job exclusion, reduced motion with elapsed status, safe settings callbacks after closing and all six legal/support documents. A review found and fixed a callback lifetime issue after window destruction during export; another regression prevents storage jobs from overlapping diagnostic exports. These checks are not Orca certification.
- The normal-build offline fixture denies non-Unix socket creation and io_uring setup in the tested process, verifies IPv4/IPv6 denial and Unix-socket availability, then runs the CLI temporary-file test and private-X11 desktop startup. The sanitizer offline fixture runs the guarded CLI; GTK controls run separately with leak detection disabled only for toolkit process-wide caches. Core/storage leak detection remains enabled. Existing OS services are outside the process filter; read-only UDisks metadata discovery may occur, with no block-device operation.
- The newly extracted package passed version/metadata/icon checks, hardened ELF/resource/document equality, pre-extraction archive/permission checks and a disposable same-version payload overlay/removal/reinstall that preserved separate user-data sentinels. This fixture does not execute a real dpkg installation, resolve dependencies, test cross-version upgrades or qualify a clean distribution VM.
- One recorded extracted-artifact offline run measured **CLI 0.0585 s / 19,736 KiB peak RSS** and **desktop 2.6539 s / 317,220 KiB peak RSS** under the guard, within provisional 10 s / 512 MiB smoke limits. The desktop includes its intentional 2-second close delay and private software rendering. These are host smoke measurements, not cold-start, idle CPU, interaction latency or low-end hardware acceptance. Later artifact reports can vary.
- A deliberately incomplete disposable DEB missing `PRODUCT_STANDARDS.md` was rejected; the original artifact remained unchanged. Negative guard-command and invalid-budget checks returned failure; unsupported capabilities are explicitly skipped with status 77. Signing with no supplied fingerprint is rejected before key access. No key or signature was generated.
- Private visuals covered Activity at 1120×900 light and 800×600 dark, and Check download at 1120×900 dark and 800×600 light. Wrapping, persistent labels and compact scrolling showed no horizontal overlap. [Activity](activity.png) and [compact checksum](check-download-small.png) capture the updated screens. Full keyboard/Orca, high contrast and scaled-font acceptance remain pending.
- Shell syntax, Python compilation, local links, source hygiene, AppStream and desktop metadata validation passed. The existing AppStream uppercase-ID pedantic hint is unchanged. Packaging/CI now emit guarded offline evidence, a direct-runtime SPDX inventory, unsigned source/artifact manifest and checksums. Optional signing requires an explicitly selected existing publisher key; unsigned integrity records do not authenticate the publisher.

No attached USB was written, mounted, unmounted, formatted or otherwise operated on; user app processes and real diagnostic files were not controlled or cleared. Core tests and UI fixtures use isolated state/temporary files. No host network setting or system installation was changed. This remains a preview: the [release checklist](../RELEASE_CHECKLIST.md) and standards assessment identify real storage, firmware, clean installation, accessibility, current dependency/security review, publisher/contact/signing and support qualification still outstanding. Exact final hosted CI success has not been observed here.

## Automated end-to-end rerun and shutdown fix — 26 October 2026

A fresh automated run initially passed all eight standard and sanitizer suites, but an additional full-process GUI trial exposed a real shutdown race: the safe test succeeded, then closing the last window could exit while a drive-discovery thread was still traversing protected filesystem metadata. A captured stack reached `pseudo_filesystem` in that worker during process teardown. An initial graphics hypothesis was not the diagnosed cause.

Discovery now holds the application alive until its result is dispatched on the GTK thread, even if the window has closed or discovery fails. Thread-start errors release the hold too. Six permanent GTK assertions exercise the actual application loop with a gated synthetic query, closing the final window before releasing both successful and failed queries. These regression cases neither discover nor open physical devices.

- After the fix, all **eight normal suites passed in 5.08 seconds** and all **eight AddressSanitizer/UndefinedBehaviorSanitizer suites passed in 5.58 seconds**. Coverage is **248 core + 122 storage + 329 GTK selection + 145 desktop-workflow assertions = 844 assertions**, plus CLI and offline fixtures. The builds reported no compiler warnings or sanitizer findings. GTK cache leak-check exclusions remain as documented above.
- Five additional launches of the actual desktop executable in private X11/D-Bus sessions activated all seven navigation actions and the real **Try a safe test** action. Each wrote, synchronized and verified **8,388,645 bytes**, produced SHA-256 `7e924d8131236a13f61eb910089790a246d21aefcf7117c6f96565af50bf91be`, removed its test output, and closed through the normal application action with exit status zero. Internet sockets were denied in the tested process. A full sanitizer desktop trial also passed with the headless Cairo renderer and toolkit cache leak detection disabled. An earlier sanitizer/default-renderer startup timed out in the private display; it is not counted as a pass.
- The optional synthetic BIOS test wrote and verified a new 4 MiB regular image, booted it in local QEMU/SeaBIOS, observed exact `KAVRUNE_BOOT_OK` and expected exit status 33, and checked that its backing image remained unchanged. The first QEMU invocation could not load local libraries; retrying with its existing project-local library and firmware paths passed without installing anything.
- Repository hygiene, AppStream and desktop-entry validation passed. The previously generated artifact hashes verified before replacing the package with the shutdown fix. Headless graphics/portal service diagnostics were observed; no application GTK critical was reported in the successful GUI trials.

No attached pendrive was written, formatted, mounted or unmounted. Only generated temporary regular files were used for write/readback/boot tests. The user's running application was not controlled. Actual USB authorization, formatting, backup/restore, distribution installation and real firmware boot qualification remain separate acceptance work.

## Beta notices and repository review — 26 October 2026

The repository-only follow-up makes beta/development status and unfinished features explicit, strengthens the existing data-loss/media/device-damage notices while preserving MIT permissions and applicable-law qualifications, and adds [INSTALL.md](../INSTALL.md). The guide covers verified download, dependency installation, ordinary-user launch, the safe test, manual updates, removal and troubleshooting. Fresh builds install the guide and embed the updated existing legal documents. Published Preview 1 assets remain tied to their original tagged source; only their release-page beta/warranty wording is clarified.

The focused [repository review](REPOSITORY_REVIEW.md) records existing GitHub/community files, source hygiene, hosted protections and remaining acceptance work. Private vulnerability reporting, dependency security updates, secret scanning and push protection were verified enabled; current hosted alert lists were empty without claiming comprehensive security or dependency coverage. PPA account/upload/key preparation was deferred at the maintainer's request.

All **eight local normal suites passed in 4.95 seconds** and all **eight local sanitizer suites passed in 5.13 seconds**, retaining **844 core/storage/GTK assertions** plus CLI/offline fixtures. No compiler warning or sanitizer finding appeared in those build logs. GitHub YAML syntax, updated local documentation links, publication hygiene and diff checks passed. The new guide is required by both repository and package-content checks. These checks use temporary files/private UI sessions and do not operate on an attached pendrive or install a system package. The follow-up PR and merged revision require their own hosted CI result; the prior nine-suite hosted evidence below is scoped to its listed revisions.

The development DEB also passed extraction, current document and embedded legal-resource equality, ELF hardening, metadata, safe-permission, disposable payload lifecycle and extracted CLI self-test checks. Both extracted CLI and desktop started with internet socket creation denied. This is package-content/offline evidence, not a system installation, authenticated publisher signature or clean-machine compatibility result. This local development package does not replace the existing public release assets.

## Published revision CI

[GitHub Actions run 40267821785](https://github.com/Cherie05/Kavrune/actions/runs/40267821785) passed for release commit `b30f1612c9b56fe2b945da35ba164597e7e7903e`. All **nine normal suites passed in 10.12 seconds**, and all **nine sanitizer suites passed in 8.67 seconds**, including synthetic BIOS execution. This exact clean source produced [Preview 1](https://github.com/Cherie05/Kavrune/releases/tag/v0.1.0-preview.1). The downloaded Actions ZIP matched GitHub's artifact SHA-256; its source fingerprint matched the clean checkout. Its DEB passed an additional local extracted-package/offline check. All eight uploaded release assets matched their local SHA-256 hashes, and an unauthenticated installer download matched the tested CI package. These are release/file-based checks, with the hardware/installation limits above unchanged. The published package remains tied to its tagged source; later documentation on `main` does not replace the release's historical packaged documents.

[GitHub Actions run 40266870341](https://github.com/Cherie05/Kavrune/actions/runs/40266870341) completed successfully for commit `5ad57a7107bb387e3d1f426c07b3214708077d6c` on 26 October 2026. Both Ubuntu 24.04 builds passed all **nine CTest suites**, including the synthetic BIOS test, with sanitizers off and on. Repository hygiene, desktop metadata, desktop smoke, Debian packaging, extracted-package lifecycle/offline checks and artifact upload also passed. The downloadable CI artifact records a clean source fingerprint for that commit. This is hosted evidence for that exact revision; the release notes identify the run used for each published artifact.

[GitHub Actions run 38540398428](https://github.com/Cherie05/Kavrune/actions/runs/38540398428) completed successfully for commit `29b3875073378c28a4ff0c6421b825f066e0fe7c`, created on 14 October 2026 at 14:40 IST. This is evidence for that published revision. Subsequent beginner UI, safe self-test, storage-protection and image-compatibility changes require their own CI run after publication; the earlier green run does not validate those changes.

Reproduce release/file tests with `scripts/build.sh`, optional synthetic boot with `scripts/test-boot.sh build/kavrune-cli`, and packaging with `scripts/package.sh`. For a sanitizer build, configure a separate build directory with `-DKAVRUNE_SANITIZERS=ON`. Refer to `VALIDATION.md` for the outstanding release gates.
