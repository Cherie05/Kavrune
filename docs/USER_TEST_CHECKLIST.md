# Test Kavrune with a spare USB

Use this checklist to record what actually works on your computer. Automated file tests do not test USB access, Linux authorization, or whether an installer starts on your hardware. A completed checklist qualifies only the recorded environment; it is not a production or enterprise certification.

## Before starting

- Use a spare USB and copy anything important from it to another drive. **Create USB and Format USB erase the selected USB.**
- Download a complete, USB-ready Linux image from its publisher. Keep it on your computer, not on the USB you will erase. If the publisher supplies a checksum, compare it in **Check download**.
- Open Kavrune as your normal desktop user. Use the Linux authorization prompt when requested; do not start the whole app with `sudo`.
- To make an installer, use **Create USB**. You do not need to format the USB first or change partition or filesystem settings.
- If you have several external drives, disconnect ones you are not testing. Match the intended USB's name and capacity before any confirmation.

Record the app version/commit, Linux distribution, desktop session, USB model/capacity, image filename and SHA-256, and intended boot computer. Review serial numbers and local paths before sharing logs.

## Check the app without erasing anything

| Check | Expected result |
|---|---|
| Click **Try a safe test** | **Safe test passed**. The test says it used temporary files and changed no USB. |
| Select a USB, visit another tool, return, and click **Refresh** | The chosen drive, its details, and the next-step message agree. The same physical drive stays selected while connected. |
| Disconnect the selected USB while no operation is running | Selection clears. Erase, backup, and scan actions are disabled. Another drive must not be silently selected. |
| Choose a complete supported Linux image | The filename and size appear. The app explains whether it can create a USB from that file. |
| Open **Check download**, select the publisher's algorithm, paste the matching checksum, and compare | **Checksum matches**. The calculated value is identified by its algorithm. |
| Compare with a deliberately different, correctly sized checksum | **Checksum does not match** and USB creation is blocked. Compare with the correct value to resolve the mismatch. |
| Open **Check USB**, review the target, and start the scan | Authorization is requested if needed, volumes are unmounted, progress appears, and the app reports the read result. No data is written. A passing read scan does not prove genuine capacity or write reliability. |
| Open **Activity**, then copy the activity | Relevant results can be copied. Errors include a useful next step and separate technical details. |

Cancelling a read scan preserves USB contents, but its volumes may remain unmounted. Open the drive again in your file manager if needed.

## Create and boot an installer

1. In **Create USB**, choose the downloaded image and your spare USB. Check that the review shows both selections and that the USB has enough capacity.
2. Click **Create USB…**. Confirm that the dialog names the intended USB. Cancel once: no writing should start and your selections should remain usable.
3. Open the confirmation again, type the displayed drive name, and confirm **Erase USB and create**. Approve the system authorization prompt if shown.
4. Leave the USB connected through writing, synchronization, and readback verification. Writing reaching 100% is not completion. Expect **Written and verified**, not just a full progress bar.
5. Use your desktop's safely remove/eject action. Connect the USB to the intended computer, restart, and choose the USB in that computer's boot menu. Follow the manufacturer's instructions for the boot-menu key and firmware settings.
6. Record whether the live system, installer, or expected login screen starts. Stop there unless you intend to install an operating system; installation can change that computer's disks.

For Alpine Linux, reaching its text login is an expected boot result. A graphical desktop is not required. For any image, **Written and verified** proves the copied image matched readback; only the separate boot test checks that computer's firmware compatibility.

## Check backup and ordinary file storage

These checks are separate from making an installer. They are not required before every USB creation.

| Tool | Action and expected result |
|---|---|
| **Back up USB** | Save to a new IMG file on another drive with free space at least equal to the USB's full capacity. Expect a completed backup and SHA-256. The source stays unchanged. Choosing an existing file must never overwrite it. |
| **Check download** | Choose the saved IMG and calculate its SHA-256. Compare with the hash reported by backup. A match checks the saved file against that result; it does not prove the original files were healthy. |
| **Create USB** | To test restoration, choose the saved IMG and a spare USB at least as large as the backup. This erases the selected target. Expect **Written and verified**, then reopen the drive and compare the original sample files. |
| **Format USB** | After installer/backup tests are finished, choose your spare USB and a suitable data-storage format. Review the erase confirmation. Expect formatting success, then open the drive in your file manager, copy a sample file, safely eject/reconnect, and read that file back. Formatting alone creates no operating system installer. |

## Distribution and enterprise acceptance

Maintainers must record separate results for each supported distribution, package version, architecture, image, and firmware. Pending checks remain pending until evidence exists.

- Approve, deny, and cancel authorization on a disposable test environment. Denial must not report success. Test absent authorization agents and unavailable storage services.
- Exercise FAT32, exFAT, and ext4 with both advertised partition schemes on disposable media. Test missing formatter tools and invalid labels before destructive actions.
- Verify protected root/home/system storage on supported layouts. The interface must not offer protected drives as targets.
- Use a disposable VM or dedicated sacrificial hardware for unplug/reconnect races, injected I/O failures, source changes, cancellation, and interrupted formatting. Do not perform these experiments on your normal system drives.
- Test clean package installation/removal, launcher/icon integration, light/dark themes, small windows, keyboard navigation, screen-reader output, and supported scaling.
- Record BIOS, UEFI, and supported Secure Boot results separately. A test on one computer does not establish universal boot support.
- Complete the remaining security, dependency, recovery, support, and release requirements in [VALIDATION.md](VALIDATION.md).

If a step fails, stop that operation, retain the message from **Activity**, and report the app version, expected result, actual result, and safe reproduction steps. After a cancelled or failed write, treat the USB as incomplete and recreate it before relying on it.
