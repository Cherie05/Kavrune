# Kavrune

An independently implemented Linux desktop application for creating and verifying USB installation media. Written in C++20 with GTK4/libadwaita, GIO D-Bus, and OpenSSL. Project source and original icon are MIT licensed.

**0.1.0 is a development preview.** Production qualification, physical USB testing and independent security review remain pending. Kavrune is an independent project with original source and artwork; the selected project name does not imply completed trademark clearance.

![Kavrune native desktop preview](docs/preview.png)

## Start the application

From this checkout:

```sh
./scripts/run.sh
```

The compiled desktop application is `build/kavrune`; the CLI is `build/kavrune-cli`. Run as your normal desktop user. UDisks2 and your desktop's Polkit authentication agent authorize individual storage operations.

Choose an ISO/IMG/RAW image, connect an external USB drive, review its identity, and click Create. The confirmation displays the exact model, device path, capacity, and serial. Writing permanently replaces the selected drive's data and partition layout. Kavrune streams the original image, synchronizes writes, verifies SHA-256 readback, and requests a device rescan before reporting success. Firmware compatibility depends on the image supplied.

## Available features

| Operation | Behavior |
|---|---|
| Create media / restore | Raw writes of hybrid ISOs and raw disk images; original partitions and boot files preserved |
| Device discovery | UDisks2 discovery, hotplug refresh, whole-drive filtering and hardware identity checks |
| Drive protection | System/storage ancestry, configured drives, active swap, holders, read-only and internal disks excluded |
| Verification | Mandatory SHA-256 comparison after writing; errors and incomplete operations reported |
| Format drive | GPT/MBR and FAT32, exFAT, ext4 using system UDisks2 tools |
| Back up drive | Full raw backup into a new file; exclusive creation and SHA-256 calculation |
| Check image | SHA-256, SHA-512, SHA-1 and MD5; optional expected checksum comparison |
| Scan drive | Read-only scan of reported capacity; reports read errors |
| Activity | Private per-user technical event log |
| CLI | Same image engine and storage validation as desktop; JSON device/image output |
| Packaging | Desktop entry, original SVG icon, AppStream metadata, `.deb` and SPDX inventory |

Windows installer extraction/WIM splitting, NTFS formatting, compressed-image streaming, distribution-specific persistence, destructive fake-capacity testing, and automatic Linux/Windows installer boot qualification on BIOS/UEFI are **not implemented**. Optical-only and Windows installation images are blocked from raw USB creation. A read-only scan cannot establish actual flash capacity. See [release roadmap](docs/ROADMAP.md).

Only external drives with verifiable identities are eligible. Some adapters expose insufficient serial information and will be refused. Filesystems whose physical ancestry cannot be established are refused where necessary for safety. There is no internal-disk override.

## Build

Source targets Ubuntu 24.04+ with GCC 12+, CMake 3.22+, GTK 4.12+, libadwaita 1.4+, OpenSSL 3 and UDisks2 2.9+. Distribution support requires validation; a package built on Ubuntu 26.04 may depend on newer runtime libraries than Ubuntu 24.04 provides. Build releases on the oldest supported distribution.

Standard development packages on Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config libgtk-4-dev libadwaita-1-dev \
  libssl-dev udisks2 polkitd dosfstools exfatprogs e2fsprogs
./scripts/build.sh
```

For development without installing headers system-wide, the helper can download and extract development packages into the ignored `.deps` directory on a host with matching runtime libraries:

```sh
python3 scripts/local-dev-deps.py
./scripts/build.sh
```

The local-header approach is for development on a matching Ubuntu host; it is not a portable SDK. Normal builds use distribution packages. Every build runs the regression tests.

```sh
./scripts/package.sh
```

Packages and the dependency inventory appear under `dist/`. Installing the package is a separate user action. It installs no root application, custom daemon, setuid executable, permissive Polkit rule, or downloaded bootloader.

## Safe test without a USB drive

```sh
./build/kavrune-cli inspect path/to/image.img
./build/kavrune-cli simulate path/to/image.img /tmp/new-test-drive.img
./build/kavrune-cli checksum /tmp/new-test-drive.img
```

Simulation requires a new regular-file destination and always verifies the written bytes. It never opens a physical storage device. Existing files and symlink image sources are refused.

Raw image writing replaces the image-length prefix of the drive. It is not a secure full-drive erase; older data and signatures beyond that length may remain. Release qualification includes checking stale partition signatures and media prepared by other tools.

```sh
ctest --test-dir build --output-on-failure
./build/kavrune --smoke-test
./scripts/test-boot.sh ./build/kavrune-cli
```

Regression tests use synthetic data in temporary files, not physical drives. `--smoke-test` briefly opens the desktop UI and exits without changing storage. The regression suite covers corruption, cancellation, image mutation, identity changes, image metadata bounds, overwrites and symlinks. The optional boot script uses system QEMU/SeaBIOS to boot an original synthetic MBR image after writing/verifying it; it skips when QEMU is unavailable. Real Linux/Windows installer booting, USB writing, format tooling, unplug behavior, and Polkit prompts require the disposable-hardware/VM release matrix in [VALIDATION.md](docs/VALIDATION.md).

## CLI

```sh
./build/kavrune-cli --help
./build/kavrune-cli devices --all --json
./build/kavrune-cli inspect image.iso --json
./build/kavrune-cli checksum image.iso --algorithm SHA256 --expected HEX_DIGEST
```

Destructive CLI commands require both the stable drive ID from `devices` and an exact matching `--confirm /dev/DEVICE` path. CLI authorization still requires a working Polkit agent. Consult `--help` for write, format, backup and scan syntax. SHA-1 and MD5 are compatibility checksums, not modern authenticity verification.

Logs are stored at `$XDG_STATE_HOME/kavrune/events.log` or `~/.local/state/kavrune/events.log`, with a private directory and file. Diagnostic details may contain local device identifiers or paths; review them before sharing.

The GUI uses gettext for localization; translations and a complete accessibility audit are still release work. See the [architecture](docs/ARCHITECTURE.md) and [validation requirements](docs/VALIDATION.md) for implementation details and release limits.

## Open source and contributing

Original project code, documentation and artwork are available under the [MIT License](LICENSE), copyright 2026 Kavrune contributors. Dependencies retain their own licenses; see [third-party notices](THIRD_PARTY_LICENSES.md) and the [licensing guide](docs/LICENSING.md).

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a change and follow the [Code of Conduct](CODE_OF_CONDUCT.md). Public issues are suitable for ordinary bugs and feature requests. Use the private reporting process in [SECURITY.md](SECURITY.md) for vulnerabilities, and review the [storage disclaimer](DISCLAIMER.md) before writing a drive.

See the [changelog](CHANGELOG.md) for preview scope and the [GitHub publishing guide](docs/PUBLISHING.md) to publish this checkout to a repository you own. No hosted repository or security contact is assumed by this checkout.
