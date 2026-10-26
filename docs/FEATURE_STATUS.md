# What works and what is pending

**Kavrune 0.1 is a development preview.** An implemented operation is not automatically a qualified feature. The table separates the code available today from the checks recorded in this workspace. See [BUILD_RESULTS.md](BUILD_RESULTS.md) for the test record and [GETTING_STARTED.md](GETTING_STARTED.md) to try it.

| Feature | Implemented | Validation and remaining work |
|---|---|---|
| Safe first test | Generate a sample image and write/verify a temporary file through the shared writer; available in the app and CLI | No USB or installer download needed. This tests file-based writing, not hardware, authorization or firmware. See the [test record](BUILD_RESULTS.md) for recorded runs. |
| Create Linux media | Raw writing of recognized hybrid ISOs, including BIOS/UEFI section catalogs, sector-zero GPT carriers, enhanced ISO descriptors and ISO/UDF bridges; mandatory SHA-256 readback comparison | See [supported profiles](IMAGE_COMPATIBILITY.md) and [recorded checks](BUILD_RESULTS.md). Structural recognition and file simulation do not qualify physical USBs, installer booting or Secure Boot on intended computers. |
| Restore raw images | IMG/RAW writing; valid MBR/GPT layouts inspected | Temporary-file testing. An unstructured IMG/RAW file is accepted for restoration without claiming it is bootable. Hardware trials remain pending. |
| Protect system drives | Whole-drive selection, device identity checks, system/internal/read-only/mounted ancestry and active-use checks | Storage safety regression tests and local enumeration. Broader LUKS/LVM/RAID/Btrfs/container layouts and reconnect races require validation. Some adapters are refused if identity cannot be verified. |
| Authorization | UDisks2 and Polkit; app runs as the ordinary user | Actual approve/deny/cancel prompts, missing agents and permission failures need disposable-VM or hardware testing. |
| Format a data USB | FAT32, exFAT or ext4 with a plain-language use choice and USB name; GPT/MBR under Advanced settings; invalid options block confirmation | Options checked in regression tests. Available system utilities were queried without formatting. Actual formatting, automount races and partial failures are pending. Formatting creates blank file storage, not an installer. NTFS is not implemented. |
| Full drive backup | Raw IMG backup to a new file; existing names rejected before confirmation, available space checked before unmounting, SHA-256 reported | File-based engine tests. Backup and scan results explain how to reopen unmounted volumes. Real drives, space/error cases and restoration on hardware still need qualification. |
| Check downloads | SHA-256/SHA-512 in the app, with the calculated algorithm identified in results and copy actions; optional expected-value comparison and checksum-only fallback for complete unsupported files | A fallback selection stays blocked for USB writing; unfinished-download suffixes remain blocked. A matching hash does not establish boot support. Users obtain trusted values themselves; automatic publisher checksum/signature workflows are pending. CLI SHA-1/MD5 serve legacy comparison only. |
| Read test | Read the drive's reported size and report read failures | File-based read tests. It does not check actual flash capacity, write reliability or future health. Hardware and error-injection tests remain. |
| Friendly desktop UI | Seven tasks with preparation and boot instructions, an explicit USB-choice placeholder, synchronized drive selection, confirmation and progress | Selection refresh preserves the same physical device, clears disconnected targets, and never implicitly picks another drive. UI checks are recorded separately from storage qualification. Full screen-reader, keyboard, contrast/scaling and translation work remains. |
| Packaging | Debian package, desktop entry, icon and AppStream metadata | Local build/package checks recorded. Clean installation/removal and runtime compatibility on each supported distribution remain pending. |
| Diagnostics and data ownership | Private bounded current/previous logs, UTC/severity, retained-history reload, copy/export to new file, clear with confirmation and CLI equivalents | Temporary-file regressions cover bounds, Unicode/control sanitization, links, competing processes and clearing. Exports remain sensitive; no automatic anonymization or upload. Logical clearing is not forensic erasure. |
| Legal and privacy | Terms, privacy notice, disclaimer, MIT and third-party/support documents packaged and embedded in an offline searchable reader | Actual behavior documented without forced acceptance or added MIT restrictions. Operator/contact and jurisdiction-specific review remain release work; notices do not establish legal certification. |
| Security/release governance | Security policy, contribution files, dependency inventory and automatic extraction checks for package notices, embedded legal documents and executable hardening | Source audit fixed optional-log blocking and backup destination checks; see [production audit](PRODUCTION_AUDIT.md). Independent security review, current dependency/license audit, complete SBOM, reproducible releases and release acceptance remain pending. |

## Features still to implement

- Windows installer USB creation, including file extraction, large WIM handling and supported BIOS/UEFI boot paths.
- Linux persistence, so a live USB can retain changes across restarts, with support designed and tested for each distribution.
- Streaming compressed images and compressed backups with bounded output sizes.
- A destructive capacity test that detects counterfeit flash or wrapped addresses, plus write diagnostics.
- NTFS formatting and additional packaging/architecture support, such as RPM and ARM64.
- Automatic trusted download checks and broader compatibility guidance backed by boot test results.

## Work required before production or enterprise use

The first priority is qualification of the Linux raw writer on disposable USB hardware and representative installer images. Test authorization, cancellation, source changes, unplugging, reconnects, read/write failures and interrupted formatting. Validate protected drive detection on common storage layouts. Decide and test how to handle stale partition signatures and data beyond the incoming image; raw writing is not secure erasure.

Then record BIOS, UEFI and Secure Boot results with the exact image checksum and hardware; qualify installation packages on supported distributions; complete accessibility and translations; and arrange independent storage/parser security and dependency reviews. Enterprise use also needs supported release/update practices, recovery guidance and a documented acceptance matrix. The existing no-warranty and liability notices do not replace this work.

Use [USER_TEST_CHECKLIST.md](USER_TEST_CHECKLIST.md) for a guided spare-USB trial. The full release acceptance checklist is in [VALIDATION.md](VALIDATION.md); planned development order is in [ROADMAP.md](ROADMAP.md). Passing local tests is evidence for the tested code paths, not certification that every USB or computer works.
