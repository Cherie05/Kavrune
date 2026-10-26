# Privacy notice

Applies to the Kavrune 0.1 preview application in this repository. Last updated: 26 October 2026.

Kavrune has no accounts, advertising, analytics, telemetry, automatic crash reporting, image downloader or automatic upload feature. Its application code does not send images, drive contents, logs or usage reports to the maintainers. You do not need an internet connection to write an image already available on your computer.

This notice describes the application, not the privacy practices of your Linux distribution, download mirror, repository host or other services you choose to use. It is a description of current behavior, not a claim of certification under a particular privacy law.

## Information used on your computer

| Information | Purpose and handling |
|---|---|
| Selected image or other file, its path, metadata and checksum | Inspect the file, calculate a checksum, copy it to the confirmed USB and verify the written bytes. Images are not mounted or executed by Kavrune. |
| Drive model, capacity, serial or other identity, device paths, partitions, mounts and storage topology | Show available drives, protect system storage, validate the chosen drive and perform the requested operation through local UDisks2 and Polkit services. |
| Drive contents | Read during backup, read scan or verification; overwrite only as part of the confirmed write, restore or format operation. |
| Operation results and errors | Display progress and diagnostics, and append local activity records as described below. |
| User-selected backup destination | Save an uncompressed full-drive image there. The file can include private files and residual data in otherwise unused areas. A backup is not encrypted by Kavrune. |

The system authorization dialog belongs to your system's Polkit agent. Kavrune does not ask for or store your password. Local services, the desktop file chooser, your filesystem and system diagnostics can maintain their own records independently of Kavrune.

## Local activity records

The desktop writes operation events, results, errors and drive-discovery diagnostics. The command-line application records successful write, format, backup and scan commands; inspect, checksum, simulation, self-test and diagnostics commands do not append operation records. Records may include timestamps, local paths, filenames, device identifiers, protection reasons and image checksums. New saved records use UTC timestamps and an information, warning or error level. Existing records keep their original timestamps, which earlier preview builds wrote in local time; live session lines in Activity use your local clock. Kavrune does not deliberately write image or drive contents into the event log, but filenames and errors can themselves contain private information.

The log is stored at:

- `$XDG_STATE_HOME/kavrune/events.log` when `XDG_STATE_HOME` is an absolute path.
- Otherwise, `$HOME/.local/state/kavrune/events.log` when `HOME` is an absolute path.

The Activity page displays the resolved path. Kavrune requests owner-only permissions for its state directory (`0700`) and log files (`0600`), rejects unsafe links and ownership, and does not encrypt the log. These permissions do not prevent access by your account, an administrator, backup software or a compromised system.

Saved activity persists after closing the application. Each event record is limited to 8 KiB. Kavrune rotates by size, retaining a current `events.log` and one previous `events.log.1`, each at most 256 KiB when maintained by the application. An oversized existing log is read through a bounded tail and trimmed at the next rotation. There is no age-based expiry and no logging opt-out setting. A private, empty `.events.lock` file coordinates access between processes. Optional diagnostics fail promptly when another process holds a diagnostics lock; a logging failure does not turn a completed USB operation into a failure.

The Activity page can inspect retained local records and current session activity. **Reload saved activity** reloads the saved records. **Save diagnostics to file…** saves a report to a new file you choose, including the application version and the retained records. Existing files are never overwritten. Exported reports request owner-only file permissions, are not encrypted, and can still contain private paths, device details and checksums. Review and redact a report before sharing it. The application does not upload it.

After finishing an operation, choose **Clear saved activity…** to clear both saved log generations and the current Activity view. Future operations create new records. The command-line `kavrune-cli diagnostics --clear --confirm clear` command likewise clears retained records; `kavrune-cli diagnostics` displays them and `kavrune-cli diagnostics --export FILE` saves a new report. Clearing does not delete exported reports, copies on other systems, clipboard history or system-service logs, and does not guarantee forensic erasure. Package removal preserves your local diagnostics; clear them before uninstalling if you want to remove those records.

Choosing **Copy activity** copies the displayed text to the desktop clipboard; it does not send a report. Choosing **Copy checksum** likewise copies the selected checksum. Clipboard history or other software on your desktop may retain copied text.

## Images, backups and temporary test files

Source images are left where you selected them. Successful backups remain at the destination you chose until you remove them; Kavrune creates them with owner-only file permissions. Those permissions can depend on the destination filesystem. A backup may contain the whole drive, including deleted-file remnants, and should be handled as sensitively as the original.

The safe test creates generated sample data in a private temporary directory, without opening a physical drive. Its temporary files are removed on normal completion, handled failure or cancellation. An operating-system crash or forced process termination can prevent cleanup. You control retention of downloaded images, completed backups and any files left after abnormal termination. Kavrune does not manage copies retained by other applications or services.

## Sharing and external services

No report is sent automatically. If you copy activity, attach logs to an issue or contact a maintainer, review and redact personal paths, device serials, private image names and other sensitive details first. A checksum may identify a private image even when the image itself is not attached. Avoid publishing disk images, passwords or credentials.

When you choose an external website or support link, your browser and that service handle the request under their own settings and policies. Package managers, downloads, desktop portals, filesystem mounts and backup or synchronization tools may communicate independently of Kavrune. A file path accessible on your computer can point to storage managed by another service.

For questions, follow [SUPPORT.md](SUPPORT.md); for a security concern, follow [SECURITY.md](SECURITY.md). This repository does not establish a separate hosted privacy service or a dedicated contact address. The [terms](TERMS.md), [MIT License](LICENSE) and [disclaimer](DISCLAIMER.md) describe the software's licensing and operational limits.
