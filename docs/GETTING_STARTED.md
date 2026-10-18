# Getting started with Kavrune

Kavrune turns a supported Linux installer file into a USB installer. An **ISO** is a file containing an operating system's installation media. A USB installer can start a computer and offer an operating system installation; copying an ISO onto a USB as an ordinary file does not do this.

Version 0.1 is a development preview. Start with the safe test below. For a physical trial, use a spare USB with no important files, and keep independent backups. Read the [disclaimer](../DISCLAIMER.md) before changing a drive.

## Try the app without a USB

1. Open **Kavrune** from your application menu if the desktop package is installed. From a source checkout, run `./scripts/run.sh` in the project folder.
2. On **Create USB**, click **Try a safe test**. No ISO or USB is needed.
3. Wait for the test to finish. It creates a small sample image, writes it into a temporary file, and checks the written data using the same writer as USB creation. It does not open or erase a physical drive.
4. Look for **Safe test passed**. This shows that the shared image writer and SHA-256 readback check worked on temporary files. It does not test USB hardware, authentication, or whether a computer will boot.

The same test is available in a terminal after building:

```sh
./build/kavrune-cli self-test
```

Success prints `Safe test passed. Writing and SHA-256 verification succeeded.` and `No USB drives were accessed or changed.` An error prints a reason and returns a failure status.

Run Kavrune as your normal desktop user. Do not launch the whole application with `sudo`. Linux may show a password prompt when you start a storage operation; Kavrune uses the system storage service to request access.

## Create a Linux USB installer

Use **Create USB** to make an installer. You do not need to use **Format USB**, choose a filesystem, or change partition settings first. The **Before you start** instructions and **How to start the computer from this USB** explanation appear above image selection in the app.

1. Download the Linux installer ISO from the operating system publisher. This preview accepts **hybrid ISOs**: installation files already arranged for both discs and USB drives. Kavrune checks the selected file and explains when it cannot write it. Windows installer creation is pending.
2. Optionally open **Check download**, choose your ISO, and paste the SHA-256 value published by the operating system publisher. A checksum is a file's fingerprint. A match means your file matches that value; use a trusted publisher's website to obtain it. If it does not match, check that the value belongs to that exact download and download the installer again if needed. Resolve the mismatch before writing the USB.
3. Plug in a spare USB. Copy any files you want to keep to a different drive first. The ISO must also be stored on a different drive.
4. Open **Create USB** and choose your installer file. Open **Choose a USB drive…** and select your USB by its name and size. The selection summary, drive details and review must name that same drive. Your chosen USB remains selected when visiting other tools or refreshing while it stays connected. Disconnecting it clears the selection; another drive is never silently chosen. Internal and system drives are protected; some adapters cannot supply enough identity information and are refused.
5. Review the selected file and USB. The USB must have enough space for the file. Click **Create USB…** and carefully review the drive name, size, device path and serial in the confirmation. Creating an installer replaces the USB's files and partitions.
6. Confirm only when you recognize the target. Type the drive name displayed in the confirmation, then click **Erase USB and create**. Approve the Linux password prompt if shown. The app shows five stages: checking the image, writing, saving buffered data, verifying, and finishing. Keep the USB connected through all five stages.
7. Wait for **Written and verified**. Then use your desktop's eject or safely remove action before unplugging the USB.

During **Saving to the USB — still working**, Linux is asking the drive to finish saving buffered data. This step has an animated indicator and elapsed time because the drive cannot provide a reliable remaining-time estimate. It can continue after all image bytes have been written; verification comes next. The overall progress bar reaches 100% only after the complete operation returns successfully, including the final device refresh.

See the [saving-stage preview](progress-saving.png), illustrated with an example image and USB.

If you cancel, **Stopping safely** can remain visible while buffered data is saved. Leave the USB connected until the operation stops. A cancelled or failed write can leave an incomplete installer; create it again before relying on it.

## How to know whether it worked

| Result | What it tells you |
|---|---|
| Safe test passed | The shared writer wrote and verified temporary files; no USB was tested. |
| Download checksum matched | The file matches the checksum you supplied. Its trust depends on where that checksum came from. |
| Written and verified | The written portion passed SHA-256 readback comparison with the selected image. |
| Installer starts on the intended computer | That image and USB can boot that computer with its current firmware settings. |

Readback verification cannot promise boot compatibility. To check that, restart the intended computer and use its boot menu to select the USB. Follow that computer manufacturer's boot-menu instructions. Reaching the installer or live desktop is enough for a boot check; do not start an installation unless you intend to change that computer's disks. Boot checks on one computer do not qualify every computer or Secure Boot configuration.

## Choose the right tool

- **Create USB:** make an installer from a supported image, or restore a saved raw drive image.
- **Format USB:** erase the USB and prepare empty space for ordinary files. Choose **USB name** and what you will use it for: FAT32 for broad compatibility, exFAT for individual files larger than 4 GiB, or ext4 for Linux. Partition-table choices are under **Advanced settings**. Invalid names or options show an explanation and keep **Format drive** disabled. This does not make an operating system installer and is not a secure erase.
- **Back up USB:** save the entire selected external drive to a new IMG file on another drive. Allow free space equal to the whole source drive, even if it is mostly empty. Existing filenames are rejected, and available space is checked before unmounting. The source is unmounted while read; reopen it in your file manager afterward to use its files.
- **Check download:** calculate a file fingerprint and optionally compare it with the publisher's value. Choose SHA-256 for a new comparison. The result and copy button identify the algorithm actually calculated; choosing another algorithm does not recalculate until you click **Calculate and compare**.
- **Check USB:** read the drive for unreadable areas without writing data. It may take a long time and unmounts the drive. Reopen it in your file manager afterward. A passed read test does not prove genuine capacity, write reliability, or future health.
- **Activity:** review progress and error details. Device identifiers and local paths may appear; review them before sharing a log.
- **How to use:** open the in-app steps and feature limits. **Legal & privacy** opens searchable Terms, Privacy, Disclaimer and MIT License documents, available without internet access. The same reader is linked from **About Kavrune**.

If no USB is offered, check that it is connected and click **Refresh**. Open **Activity** to read why a drive is protected or unsupported. If authentication or the storage service is unavailable, use a normal Linux desktop session with UDisks2 and a password prompt agent. From a source checkout, rebuild with `./scripts/build.sh` after changes, then launch again.

## If your image is not accepted

Read the app's next step: finish an incomplete download, extract an archive, convert a virtual disk, or use the publisher's USB preparation method. Windows, optical-only ISO and UDF-only images can be checked, but this version cannot create a bootable USB from them. Do not rename a file to bypass these checks.

To diagnose a completed download, open **Check download**, choose the file there, and compare it with the publisher's SHA-256. This also works for complete files whose format or structure the USB creator cannot accept. A **Checksum only** selection keeps creation disabled. A matching hash does not add USB support or prove the image will boot; if it matches but is still refused, follow the publisher's preparation instructions. Unfinished `.part` and similar downloads remain blocked. See the [compatibility guide](IMAGE_COMPATIBILITY.md) for message meanings and supported profiles.

After an app update or source rebuild, close Kavrune completely and reopen it so the new image checks are loaded.

## Privacy and notices

Open **How to use → Legal & privacy**, then choose a document. Use **Ctrl+F** to search, **Enter** or the arrows to move between matches, and **Close** to return. Reading the notices does not approve or start a drive operation.

Kavrune has no telemetry or automatic uploads. Activity records are local and can include paths, device identifiers and checksums. They persist until removed; the [privacy notice](../PRIVACY.md) explains deletion after finishing operations and closing the app. Review copied activity before sharing it. See the [terms](../TERMS.md), [disclaimer](../DISCLAIMER.md), and [MIT License](../LICENSE) for preview, storage and warranty information.

Use the [USB test checklist](USER_TEST_CHECKLIST.md) to record the results of each tool and the separate boot test. See [what is implemented and pending](FEATURE_STATUS.md), [recorded checks](BUILD_RESULTS.md), and the [production validation requirements](VALIDATION.md).
