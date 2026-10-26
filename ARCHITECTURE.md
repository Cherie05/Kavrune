# Kavrune architecture and decisions

The detailed [architecture guide](docs/ARCHITECTURE.md) documents the existing implementation. This root entry identifies the boundaries required by the product standards.

| Boundary | Implementation and trust |
|---|---|
| Presentation | GTK/libadwaita desktop; explicit task state, native dialogs, worker-to-main-context progress, offline legal reader and user-controlled diagnostics. Image/device labels are untrusted text, not executable markup. |
| Application/core | C++20 image inspection, bounded streaming, checksums, cancellation, readback, backups and local diagnostic files; shared with the CLI and tested using regular files. |
| Operating system | Storage layer resolves system ancestry and hardware identity, calls local UDisks2 and receives authorized descriptors. The Polkit agent owns password prompts; Kavrune stores no credentials. |
| Local persistence | User-selected ISO/backup files; bounded private XDG state logs. There is no database, remote cache, account session or persisted destructive target. |
| Build/release | CMake creates one version definition, embeds local UI/notices, builds/tests hardened executables and packages docs. Package/offline checks and an unsigned source/artifact manifest provide bounded evidence, not authentication. |

Imported images are never mounted, extracted, executed or used to load plugins. Parsing is content-based with offset/size/CRC checks. The writer holds the authorized device descriptor through flush and verification rather than reopening a mutable `/dev/sdX` name. Backup cleanup uses a pinned destination directory and checks inode identity. Local log access rejects unsafe links/types/owners and does not block on optional diagnostic locks.

Raw creation preserves the publisher's image layout. It writes only the incoming image-length prefix; it is not secure erase and does not guarantee removal of stale signatures outside that prefix. Formatting is a separate multi-step UDisks operation. These operations cannot be made transactional across power loss, so the recovery contract is to report an incomplete result and recreate media after inspection; never silently resume an old destructive job.

System UDisks/Polkit is used instead of a new root daemon. System theme/fonts/reduced-motion preferences are used instead of decorative application settings. Settings/database migration, sync conflicts and server-client interfaces are absent because there is no such data model. If future scope changes, update this document, privacy inventory, tests and release gates first.
