# Kavrune product requirements

Kavrune is a native, offline Linux desktop utility for people who need to create a Linux installer USB, restore a raw drive backup, format removable storage for files, back up a drive, compare image checksums or scan for unreadable data. The GUI serves beginners; the CLI exposes the same safety engine for informed local use. Current version: 0.1 development preview, with the build version defined in CMake.

## Supported scope

| Task | Required behavior |
|---|---|
| Create or restore USB | Accept supported hybrid ISO/IMG/RAW content, explicitly choose an eligible external drive, confirm erasure, pin source and drive identity, copy, synchronize, verify every written byte and report final success only after release/rescan. |
| Format data storage | Explain FAT32/exFAT/ext4 and GPT/MBR choices; reject invalid names/options before confirmation. Formatting creates blank storage; it does not create an installer and cannot safely be interrupted at arbitrary steps. |
| Full backup | Save to a new file away from the source, check free space, synchronize and retain correct destination identity; never overwrite an existing file or remove a replacement during cleanup. |
| Check download | SHA-256/SHA-512 and optional comparison against a user-supplied trusted checksum; unsupported complete files may be checksum-only. Byte agreement does not certify publisher authenticity or bootability. |
| Check USB | Read reported addresses without writing; explain that this is neither fake-capacity detection nor a prediction of future reliability. |
| Activity and support | Bounded local history, inspect/copy/export/clear controls with privacy guidance, actual application version, no automatic diagnostic uploads. |
| Help and legal | Local preparation/boot instructions, explicit limitations, and offline Terms, Privacy, Disclaimer and MIT/third-party notices. |

## Safety and quality acceptance

The normal application runs unprivileged; existing UDisks2 and Polkit authorize individual operations. Internal/system/active/unidentified devices remain protected without an override. The application never chooses a target silently, resumes destructive work automatically, executes an imported image, bundles downloaded OS images or claims all ISOs are supported.

Core operations do not require internet, registration or a remote server. File pickers can expose network-mounted paths managed by the OS; these are outside the promise that local-file workflows are offline. UI icons/assets are local, and desktop theme/fonts follow the installed environment.

Long jobs must stay responsive with honest progress and cancellation where feasible. Critical errors remain available with recovery guidance. Storage operations have no undo; a cancelled or failed write must be recreated. A restart must not automatically select or resume the previous destructive target. Physical device identity and image content are rechecked for each new operation.

The source target is GTK 4.12+/libadwaita 1.4+, OpenSSL 3 and UDisks2 on Linux. DEB/x86_64 is the currently produced package; source/runtime compatibility on Ubuntu 24.04+ is a target, not an already validated distribution matrix. A package built on Ubuntu 26.04 can need newer system libraries. RPM, Flatpak, AppImage, ARM64 and other platforms are not promised.

## Deliberately absent features

No application accounts, cloud sync, web API, subscriptions, payment processing, cookies, advertising, telemetry, AI, plugin marketplace, custom privileged daemon or automatic updater. No database is needed for the present data model. Do not create these systems just to check boxes.

Windows installer extraction, persistence, compressed-image streaming, NTFS formatting and destructive capacity testing are not implemented. General hardware/firmware compatibility, a supported stable release, independent security review, trademark clearance and enterprise support require separate acceptance. See [the standards assessment](docs/STANDARDS_ASSESSMENT.md).
