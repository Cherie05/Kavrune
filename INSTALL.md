# Install and use Kavrune

**BETA / DEVELOPMENT PREVIEW — under active development.** Features may be unfinished, contain bugs or fail unexpectedly. The software is supplied as is, without warranty under the [MIT License](LICENSE). Storage operations and failures can cause permanent data loss, corrupted/unusable media or device/system damage. Keep independent backups and evaluate using spare media. See [DISCLAIMER.md](DISCLAIMER.md) and [TERMS.md](TERMS.md) for the governing notices and legal limits.

## Computer requirements

- The published installer is an `amd64`/`x86_64` package for 64-bit Intel/AMD computers, built on Ubuntu 24.04. ARM computers require a separate build.
- Linux Mint 22.x is a compatibility target. Actual clean-system installation and physical USB qualification remain pending; see [validation](docs/VALIDATION.md).
- Use a normal graphical Linux session with UDisks2 and a Polkit authentication agent. The installer declares the required runtime dependencies.
- Internet may be needed to download the installer and dependencies. Local application operations work offline after installation.

Users do not need Git, a compiler or a GitHub account. The [beta PPA](https://launchpad.net/~arunvpp/+archive/ubuntu/kavrune-beta) was activated, but no package publication was observed at the 26 October 2026 setup check. Until a compatible package is published and installation-tested, use the downloaded `.deb` below. Maintainers can follow the [complete Launchpad guide](docs/LAUNCHPAD.md); an empty PPA does not make `apt install kavrune` available.

## Download and check the installer

1. Open [Kavrune 0.1.0 Preview 1](https://github.com/Cherie05/Kavrune/releases/tag/v0.1.0-preview.1).
2. Under **Assets**, save `kavrune_0.1.0_amd64.deb` and `SHA256SUMS` into the same folder, such as Downloads. Source archives and JSON reports are not needed to install the app.
3. Open Terminal in that folder. For the usual Downloads location:

   ```sh
   cd ~/Downloads
   sha256sum --check --ignore-missing SHA256SUMS
   ```

4. Check that the result includes `kavrune_0.1.0_amd64.deb: OK`. The command skips supporting assets you did not download. If verification fails or no file is verified, download the matching files again before installing.

The current release's checksums and provenance are unsigned. They compare downloaded bytes with the published list; they do not independently authenticate a publisher signing key. Obtain both files from the official release linked above.

## Install and open

From the folder containing the installer:

```sh
sudo apt update
sudo apt install ./kavrune_0.1.0_amd64.deb
```

Enter your Linux password; password characters normally remain invisible. Confirm with **Y** if asked. The `./` selects the downloaded file; APT resolves the declared dependencies. This process is documented in [Ubuntu's package installation guide](https://documentation.ubuntu.com/project/contributors/bug-fix/install-built-packages/).

Search for **Kavrune** in the application menu, or run:

```sh
kavrune
```

Run the app as your ordinary desktop user. Do not launch it with `sudo`. Linux may request authorization separately when you confirm a storage operation. To check the installed version:

```sh
kavrune --version
```

The published Preview 1 binaries report `0.1.0 preview`. Beta describes the same development/testing status; it does not claim a stable release or change the historical release's package version.

## First test without a pendrive

Open **Create USB → Try a safe test** and wait for **Safe test passed**. The test writes, synchronizes and verifies temporary regular files; no physical USB is opened or changed.

After installation, the terminal equivalent is:

```sh
kavrune-cli self-test
```

A passing safe test checks the file-based writer. It does not qualify USB hardware, authorization, formatting or firmware boot. Follow the [beginner guide](docs/GETTING_STARTED.md) before a physical trial. Creating or formatting a USB erases its existing data; keep it connected until the operation finishes.

## Update and remove

Updates are manual: download the newer release's matching installer/checksums, verify them, and install its `.deb` using the same method with the actual new filename. Close Kavrune after any active operation has safely finished before updating. Do not expect automatic Kavrune updates from Ubuntu's repositories until a supported repository has been published and configured.

To uninstall:

```sh
sudo apt remove kavrune
```

Your ISO files, backups, exported reports and local diagnostics remain. If desired, use **Activity → Clear saved activity…** before uninstalling; exported copies are unaffected. See [PRIVACY.md](PRIVACY.md) for data retention and deletion.

## Installation problems

- **Unable to locate package kavrune:** use the downloaded file command, including `./` and the complete `.deb` filename. No project APT repository/PPA is published yet.
- **Installer file not found:** open Terminal in the folder where the browser saved the file. Use its actual filename if the browser added a suffix.
- **Wrong architecture or unavailable dependencies:** check the Linux release and CPU architecture. Do not force-install incompatible packages or replace system libraries to bypass the error; report the environment through [SUPPORT.md](SUPPORT.md).
- **The app refuses to run as root:** open it from the application menu or run `kavrune` without `sudo`.
- **No USB is listed or no authorization prompt appears:** consult **How to use** and **Activity**. Protected or unidentified devices remain unavailable; using root is not a workaround.

For ordinary bugs, use the repository's [issue forms](https://github.com/Cherie05/Kavrune/issues/new/choose) after removing personal data from diagnostics. For vulnerabilities, follow [SECURITY.md](SECURITY.md) and report privately.
