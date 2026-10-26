# Storage safety and security

## Supported versions

| Version | Security support |
|---|---|
| Latest `main` / current 0.1 development preview | Best-effort investigation and fixes |
| Older snapshots and modified third-party builds | No maintained support commitment |

Kavrune has no stable release, dedicated security team or response-time SLA. Production qualification is pending; see [validation requirements](docs/VALIDATION.md). Fixes may require updating to the current preview.

## Reporting a vulnerability

The official repository has [private vulnerability reporting](https://github.com/Cherie05/Kavrune/security/advisories/new) enabled. Open its **Security** tab and choose **Report a vulnerability**, or use that link to submit your report privately. This hosted setting was verified on 26 October 2026. Forks must configure their own reporting channel; a copied policy does not enable the feature.

If no private channel is listed, ask the maintainer to provide one without posting exploit details, credentials, disk contents or personal information in a public issue. Describe the affected version or commit, distribution, observed behavior, impact and safe reproduction steps using synthetic files or a disposable VM. Include device identifiers only when necessary and redact personal details from logs.

Give maintainers a reasonable opportunity to investigate and coordinate a fix before public disclosure. Reports are reviewed on a best-effort basis; do not run destructive demonstrations against another person's devices or data.

If GitHub's reporting service is unavailable, ask the maintainer for an alternative private channel without sharing sensitive details. No fallback email address or response-time commitment is established. Instructions for repository owners are in [GitHub's private reporting guide](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).

## Storage safety model

Kavrune performs destructive operations only after target-specific confirmation in the desktop or explicit ID/path confirmation in the CLI.

The GUI and CLI run unprivileged and refuse normal operation as root. The existing system UDisks2 service and Polkit agent handle authorization. The app adds no authorization bypass and exposes no shell execution interface.

Before raw writing, the storage layer excludes internal/system devices, resolves filesystem ancestry through sysfs, checks active swap/holders/configuration, verifies that the source image is stored away from the target, unmounts the drive, and revalidates hardware identity. Authorization returns an exclusive block-device descriptor; its type, device number and capacity are checked. The writer keeps that descriptor through synchronization and readback rather than reopening a mutable `/dev/sdX` name.

Identity includes hardware ID, serial, object paths, kernel device number, sysfs path, capacity, vendor/model, connection, removable state and detection timestamps. A disconnect/reconnect invalidates the selection. Kavrune fails closed when it cannot establish required topology or identity. This intentionally excludes some hardware.

The source must be a nonempty regular file; terminal symlinks, compressed/container formats and malformed metadata are refused. Inspection has bounded reads and integer/offset checks. The writer pins source identity and checks it before and after writing. Image files are not mounted, extracted or executed.

Formatting uses only fixed filesystem/partition enums and constrained labels through documented UDisks methods. UDisks formatting is a multi-step operation; identity is checked between steps. Unlike raw writing, its service calls cannot be atomically tied to a retained exclusive descriptor. Cancellation waits for each synchronous filesystem operation to finish. A failed or cancelled format can leave a partially prepared drive.

Cancellation or an I/O error leaves potentially incomplete installation media. Accepted writes are synchronized before control returns where the device permits it; cancellation cannot repair erased data. USB firmware, failing hardware, power loss, operating-system bugs and a compromised privileged service remain outside the application's guarantees. Never interpret a verified byte stream as proof that an arbitrary image boots on every firmware.

Backups refuse overwriting existing files and remove a partial file only when its identity still matches the created file. Logs use private permissions and reject symlinks/hardlinks. Local files are not uploaded; the app has no telemetry or image download feature.

The safeguards reduce risk; they cannot guarantee that an operation will never lose data or damage a storage layout. Read the [disclaimer](DISCLAIMER.md), keep backups outside the target drive, and complete the release matrix before presenting a build as production qualified.
