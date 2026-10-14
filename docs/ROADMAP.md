# Release roadmap

The independently implemented 0.1 milestone delivers the raw Linux media workflow, format, backup, checksums and a read scan. The following milestones describe the remaining work toward 1.0.

| Milestone | Work required |
|---|---|
| 0.1 release qualification | Disposable USB matrix, VM boot tests, Polkit/unplug coverage, package portability, independent security review |
| 0.2 Windows installers | Bounded ISO/UDF extraction, FAT32 UEFI plan, system wimlib splitting, large-file verification, legitimate user-provided boot files |
| 0.3 persistence and streaming | Distro-specific persistence plugins, gzip/xz/zstd streams with output bounds, optional compressed backups, trusted upstream checksum workflow |
| 0.4 hardware diagnostics | Deterministic destructive capacity tests, wrapping/corruption detection, explicit destructive confirmation, bad-block/performance reports |
| 1.0 | Qualified Linux/Windows boot matrix, supported filesystem/firmware combinations, full accessibility/translations, reproducible packages, dependency audit, SBOM, security review |

NTFS, BIOS Windows boot paths, automated installer customization, Flatpak host-service architecture, RPM and ARM64 are separate planned work. No downloaded proprietary boot assets are bundled.

Only describe an implemented, tested capability as supported. A release number must not substitute for evidence. Kavrune is the selected name; trademark clearance remains a release requirement.
