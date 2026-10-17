# What works and what is pending

**Kavrune 0.1 is a development preview.** An implemented operation is not automatically a qualified feature. The table separates the code available today from the checks recorded in this workspace. See [BUILD_RESULTS.md](BUILD_RESULTS.md) for the test record and [GETTING_STARTED.md](GETTING_STARTED.md) to try it.

| Feature | Implemented | Validation and remaining work |
|---|---|---|
| Safe first test | Generate a sample image and write/verify a temporary file through the shared writer; available in the app and CLI | No USB or installer download needed. This tests file-based writing, not hardware, authorization or firmware. See the [test record](BUILD_RESULTS.md) for recorded runs. |
| Create Linux media | Raw writing of recognized hybrid ISOs, including BIOS/UEFI section catalogs, sector-zero GPT carriers, enhanced ISO descriptors and ISO/UDF bridges; mandatory SHA-256 readback comparison | See [supported profiles](IMAGE_COMPATIBILITY.md) and [recorded checks](BUILD_RESULTS.md). Structural recognition and file simulation do not qualify physical USBs, installer booting or Secure Boot on intended computers. |
| Restore raw images | IMG/RAW writing; valid MBR/GPT layouts inspected | Temporary-file testing. An unstructured IMG/RAW file is accepted for restoration without claiming it is bootable. Hardware trials remain pending. |
| Protect system drives | Whole-drive selection, device identity checks, system/internal/read-only/mounted ancestry and active-use checks | Storage safety regression tests and local enumeration. Broader LUKS/LVM/RAID/Btrfs/container layouts and reconnect races require validation. Some adapters are refused if identity cannot be verified. |
| Authorization | UDisks2 and Polkit; app runs as the ordinary user | Actual approve/deny/cancel prompts, missing agents and permission failures need disposable-VM or hardware testing. |
| Format a data USB | GPT/MBR with FAT32, exFAT or ext4 via system tools | Options checked in regression tests. Actual formatting, automount races and partial failures are pending. Formatting creates blank file storage, not an installer. NTFS is not implemented. |
| Full drive backup | Raw IMG backup to a new file; SHA-256 reported; overwrites refused | File-based engine tests. Real drives, sufficient-space/error cases and restoration on hardware still need qualification. |
| Check downloads | SHA-256/SHA-512 in the app, plus legacy SHA-1/MD5 in the CLI; optional expected-value comparison and checksum-only fallback for complete unsupported files | A fallback selection stays blocked for USB writing; unfinished-download suffixes remain blocked. A matching hash does not establish boot support. Users obtain trusted values themselves; automatic publisher checksum/signature workflows are pending. SHA-1/MD5 serve legacy comparison only. |
| Read test | Read the drive's reported size and report read failures | File-based read tests. It does not check actual flash capacity, write reliability or future health. Hardware and error-injection tests remain. |
| Friendly desktop UI | Native GTK4/libadwaita pages, task instructions, confirmation and progress | UI checks are recorded separately from storage qualification. Full screen-reader, keyboard, contrast/scaling and translation work remains. |
| Packaging | Debian package, desktop entry, icon and AppStream metadata | Local build/package checks recorded. Clean installation/removal and runtime compatibility on each supported distribution remain pending. |
| Security/release governance | MIT license, disclaimer, security policy, contribution files and dependency inventory | Independent security review, current dependency/license audit, complete SBOM, reproducible releases and release acceptance remain pending. |

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

The full acceptance checklist is in [VALIDATION.md](VALIDATION.md); planned development order is in [ROADMAP.md](ROADMAP.md). Passing local tests is evidence for the tested code paths, not certification that every USB or computer works.
