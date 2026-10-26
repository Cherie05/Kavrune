# Publish the Kavrune beta PPA

This is the complete maintainer workflow for the public [arunvpp/kavrune-beta PPA](https://launchpad.net/~arunvpp/+archive/ubuntu/kavrune-beta). The account and archive were observed active on 26 October 2026; no source package was published and no upload key was registered at that check. An active empty PPA is not an installable application. Check the live package/build pages for subsequent publication.

The first packaging target is **Ubuntu 24.04 LTS (noble), amd64/x86_64**. Linux Mint 22.x remains a compatibility target requiring separate installation testing. Do not advertise this PPA for every Linux distribution or copy binaries into unrelated Ubuntu releases. Kavrune remains an offline beta development preview, supplied as is under the MIT license; USB writes/formatting erase data. See [the disclaimer](../DISCLAIMER.md) and [release qualification gates](../RELEASE_CHECKLIST.md).

## 1. Choose your public identity

No company, domain, GitHub organization or paid server is required for an individual public open-source PPA. The maintainer name/email appear in package control files and changelogs; public OpenPGP keys also expose their identity fields. Choose a working project contact you are comfortable publishing. Account email privacy settings cannot hide an address inside downloadable source/packages. Never use GitHub noreply as the package contact. Keep Git author and committer emails on your GitHub noreply identity; package contact and Git identity are separate.

Use the same approved name/email in your signing key and generated changelog. Do not commit private keys, passphrases, GPG directories, account tokens or generated packages. The helper writes the chosen package identity only into a new private output directory. Nothing signs, creates keys or uploads automatically.

## 2. Install source-upload tools

On your Ubuntu publishing computer:

```sh
sudo apt update
sudo apt install --no-install-recommends gnupg devscripts debhelper dpkg-dev dput lintian fakeroot
```

These are maintainer tools. Ordinary app users do not need them, Git or a Launchpad account.

## 3. Create and register your OpenPGP key

If you already have a suitable publisher-controlled signing key, use it rather than creating another. Otherwise run this interactively in your own terminal:

```sh
gpg --default-new-key-algo rsa4096 --gen-key
gpg --list-secret-keys --keyid-format long --with-fingerprint
```

Enter your chosen public identity, choose a strong passphrase and retain the private key/revocation certificate privately. Find the **full primary fingerprint**, not just a short key ID. Substitute it for `YOUR_FULL_FINGERPRINT` below:

```sh
gpg --keyserver hkps://keyserver.ubuntu.com --send-keys YOUR_FULL_FINGERPRINT
```

Open [your OpenPGP key settings](https://launchpad.net/~arunvpp/+editpgpkeys), paste that fingerprint and select **Import Key**. Launchpad sends an encrypted email. Save its PGP message block to a local file and decrypt it on the same computer:

```sh
gpg --decrypt launchpad-confirmation.asc
```

Follow the link in the decrypted message to confirm ownership. Do not share that confirmation link/message publicly. Keyserver propagation can take time. Confirm the key appears on your Launchpad profile. Read and sign the [Ubuntu Code of Conduct](https://launchpad.net/codeofconduct) as requested by Launchpad's setup flow.

Your upload key authenticates source submissions. Launchpad manages the separate archive signing key used by APT; it can appear only after the first accepted upload. Do not confuse the two fingerprints.

## 4. Prepare the reviewed source

Use a clean checkout containing `scripts/prepare-ppa.py`. Review the exact commit and its hosted normal/sanitizer/PPA packaging checks before publishing. After the history cleanup, do not merge old clones/branches back into main; see [publication privacy](PUBLICATION_PRIVACY.md).

From the repository root, read the public name/email interactively so they are not literal entries in shell history:

```sh
read -r -p 'Public package maintainer name: ' KAVRUNE_PPA_NAME
read -r -p 'Public working package email: ' KAVRUNE_PPA_EMAIL
python3 scripts/prepare-ppa.py \
  --maintainer-name "$KAVRUNE_PPA_NAME" \
  --maintainer-email "$KAVRUNE_PPA_EMAIL" \
  --beta 1 --output dist/ppa-beta1
unset KAVRUNE_PPA_NAME KAVRUNE_PPA_EMAIL
```

The helper refuses dirty source, existing output, source symlinks and unsafe identity fields. It exports committed files, preserves MIT/notices and generates standard Debian `3.0 (quilt)` packaging under `dist/ppa-beta1/kavrune-0.1.0`. It does not copy ignored builds or local data. `ppa-preparation.json` records the exact source/tree/archive hash and explicitly records that preparation is unsigned and unpublished.

The initial package version is `0.1.0-0ubuntu1~beta1~noble1`. It sorts above the existing `0.1.0` preview DEB and below `0.1.0-0ubuntu1`. Launchpad does not allow replacement of a used package version. For a packaging-only retry from the same reviewed Git commit, increase `--beta` and choose a new output directory.

**An upstream archive name must never acquire different bytes.** Retain the original `.orig.tar.xz` and its SHA-256. If upstream source or documentation changes after an accepted upload, bump the CMake upstream version and matching AppStream metadata, review/test the new release, then generate a new upstream archive. Increasing `--beta` alone is not enough to replace changed upstream source under the old `.orig` filename. Even a different Git commit with identical files can change Git archive metadata; compare hashes before reusing an upstream version. Use the actual generated source directory/version in subsequent commands.

## 5. Build and sign the source package

First inspect `debian/control`, `debian/changelog`, `debian/copyright` and the preparation report. Ensure the identity is exactly what you approved for public use. Then:

```sh
cd dist/ppa-beta1/kavrune-0.1.0
debuild -S -sa -d -kYOUR_FULL_FINGERPRINT
cd ..
```

`-S` builds source only, `-sa` includes the original source archive and `-d` skips local binary-build dependency checks. The source-only clean step still uses debhelper. This is useful when the publishing computer differs from Ubuntu 24.04; Launchpad resolves target build dependencies and runs the real binary build/tests. It does not justify skipping target-build or installation checks. Review lintian results; do not hide unexpected failures.

Enter the passphrase through GPG's local prompt. Verify the signatures using your locally trusted public key:

```sh
gpg --verify kavrune_0.1.0-0ubuntu1~beta1~noble1.dsc
gpg --verify kavrune_0.1.0-0ubuntu1~beta1~noble1_source.changes
```

Both must succeed with the intended key. Review the files listed by `.changes`, including build information, for public identity/path suitability. Do not upload the entire `dist` folder. Keep the `.orig.tar.xz`, `.debian.tar.xz`, `.dsc`, `.buildinfo` and `_source.changes` together. A prebuilt `.deb` is not a Launchpad source upload.

## 6. Upload and wait for publication

From `dist/ppa-beta1`, run:

```sh
dput ppa:arunvpp/kavrune-beta kavrune_0.1.0-0ubuntu1~beta1~noble1_source.changes
```

This command publishes the signed files listed in `.changes`. Read the acceptance/rejection email. Check [PPA package details](https://launchpad.net/~arunvpp/+archive/ubuntu/kavrune-beta/+packages) and [builds](https://launchpad.net/~arunvpp/+archive/ubuntu/kavrune-beta/+builds). Require an accepted source upload, successful noble/amd64 binary build and **Published** status before announcing installation commands. Uploaded, building, failed and published are distinct states. Use the archive page's generated APT instructions once its signing key is ready.

If FTP upload fails, Launchpad also documents SFTP. Register a public SSH key with Launchpad and configure the corresponding `dput` target only if using that method; do not disable signature checks or blindly accept an unverified SSH host key. See the official upload guide below.

## 7. Test installation, then give users the commands

In a disposable Ubuntu 24.04 amd64 VM, confirm installation, application menu launch, `kavrune --version`, and the safe temporary-file self-test. Test reinstall/upgrade/remove and retained user files separately. Do not erase a physical drive as an automated publication check. Extraction/CI checks alone do not exercise APT installation.

**Run these only after this PPA has published a compatible package**, on Ubuntu 24.04 amd64:

```sh
sudo apt update
sudo apt install software-properties-common
sudo add-apt-repository ppa:arunvpp/kavrune-beta
sudo apt update
apt-cache policy kavrune
sudo apt install kavrune
kavrune
```

Verify the candidate version/origin is the expected PPA before installation. Launch the app as the ordinary desktop user; do not run `sudo kavrune`. Test with **Create USB → Try a safe test** or `kavrune-cli self-test`. Local operations work offline after packages/dependencies are installed. Users need no Git or compiler.

While the PPA is configured, later compatible versions are offered through normal APT updates; Kavrune itself has no updater. To remove the app and stop using the archive:

```sh
sudo apt remove kavrune
sudo add-apt-repository --remove ppa:arunvpp/kavrune-beta
sudo apt update
```

Removing the PPA alone does not remove an installed app. Removing the app retains user images, backups and diagnostics; see [Privacy](../PRIVACY.md).

## Evidence and limits

Local tests use disposable Git repos and real unsigned source-package build/extraction round trips, without opening drives or changing the host package database. Hosted Ubuntu 24.04 CI additionally rebuilds the generated source package through debhelper, runs its application tests and checks the resulting binary payload/offline operation. CI uses a reserved `.invalid` fixture contact and does not publish it or register a key.

These checks do not establish Launchpad acceptance, real APT lifecycle behavior, hardware compatibility, accessibility qualification or stable/enterprise support. Record observed upload/build/install results before changing the status in [INSTALL.md](../INSTALL.md). The historical GitHub preview installer remains unchanged.

Official references: [PPA creation](https://ubuntu.com/docs/launchpad/user/how-to/packaging/create-ppa/), [OpenPGP registration](https://ubuntu.com/docs/launchpad/user/how-to/import-openpgp-key/), [source upload and SFTP](https://ubuntu.com/docs/launchpad/user/how-to/packaging/ppa-package-upload/), [source build options](https://manpages.ubuntu.com/manpages/noble/man1/dpkg-buildpackage.1.html).
