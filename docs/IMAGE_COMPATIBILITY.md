# Which image files can Kavrune use?

For **Create USB**, choose the completed USB-ready image from the operating system publisher. Kavrune copies the image's existing disk layout; it does not turn every disc image into a USB installer. The file extension alone does not establish compatibility.

**Check download** can compare a file with the publisher's checksum even when that file cannot be written by this version. A matching checksum means the downloaded bytes match the supplied value. It does not establish USB boot support, firmware compatibility or Secure Boot qualification.

## Supported image profiles

| Image or metadata profile | What Kavrune does |
|---|---|
| Hybrid ISO with a valid MBR or GPT disk layout | Allows raw writing and mandatory readback verification. Embedded boot information is preserved; the intended computer still needs a boot test. |
| BIOS, UEFI-only, or combined El Torito boot catalog | Recognizes boot entries in the default entry and later sections, including a usable UEFI entry after an unused BIOS entry. Catalog validation, section counts, extensions and referenced byte ranges remain checked. |
| UEFI El Torito entry with a load-sector count of zero or one | Recognizes the EFI convention in both default and section entries. The boot-image start must remain inside the image. BIOS entries still require a nonzero, bounded load size. |
| Hybrid MBR carrier with generator-selected partition type | Recognizes valid nonzero partition types and the bounded type-zero whole-image convention. A type-zero one-sector placeholder needs a valid GPT layout; the placeholder alone does not establish USB compatibility. |
| Hybrid GPT carrier starting at sector zero | Allows the first populated GPT entry to cover the ISO and its metadata after validating the ISO, boot catalog and MBR carrier. Its nonzero type GUID may be chosen by the image generator; unused entry slots may precede it. Remaining entries, CRCs and image/usable-range bounds stay checked. Ordinary raw GPT images retain strict lower bounds. |
| ISO enhanced volume descriptor | Accepts type 2/version 2 alongside the required primary volume descriptor. Unknown descriptor versions are rejected. |
| ISO/UDF bridge | Recognizes a complete UDF recognition sequence before or after ISO descriptors, including permitted `BOOT2` records. The ISO and disk layout must independently qualify for USB creation. |
| MBR/GPT disk image or unstructured IMG/RAW backup | Allows raw restoration. Partitioned images are inspected; an unstructured backup does not receive a bootability claim. Use images whose source and purpose you trust. |
| Detected Windows installation ISO | Can be inspected and checksummed. Raw USB creation is blocked because Windows needs file extraction, large-file handling and a supported boot setup, which remain unimplemented. |
| Optical-only ISO or UDF-only disc image | Can be inspected and checksummed. Raw USB creation is blocked; obtain a USB-ready image or follow the publisher's preparation method. UDF recognition is not full UDF filesystem validation. |
| Compressed download or archive | Extract it first, then select the ISO, IMG or RAW file inside. Direct compressed writing is not implemented. |
| Virtual disk container | Convert it to a raw disk image with a trusted tool first. Recognized containers include QCOW, VHD/VHDX, VMDK and VDI. |

These are structural profiles, not a promise to support every distribution or image version. See the [recorded checks](BUILD_RESULTS.md) for exact tested images and test scope.

## If an image is refused

| App message | Next step |
|---|---|
| **Download not finished** | Wait for completion and select the final file. Browser/download suffixes such as `.crdownload`, `.part`, `.partial` and `.download` remain blocked. Do not rename an unfinished download to bypass the check. |
| **Extract this download first** | Extract the archive and choose the image inside. |
| **Convert this disk image first** | Convert the virtual disk to raw format; changing the filename does not convert its contents. |
| **Check this download** | Compare the exact file's SHA-256 with the publisher's value. If it differs, download it again. If it matches, consult the publisher's USB instructions and report the compatibility issue with the image version and checksum. |
| **Different creation method needed** | The file is recognized but requires a USB preparation method this version does not provide. Check its checksum or obtain a USB-ready alternative. |
| **Image needs another creation method** | Inspection reached a metadata limit. Follow the publisher's instructions; the limit is not proof the image is corrupt. |
| **Choose a disk image** | Select the actual completed image. To check another kind of file, choose it from **Check download**. |

Choosing a complete unsupported, unrecognized or structurally invalid file from **Check download** enables a checksum-only fallback. It keeps **Create USB** disabled for that selection. A checksum can help diagnose a bad download without relaxing write checks. Unfinished-download suffixes remain blocked in this workflow, and missing, empty, unreadable or changing files can still fail.

Malformed images remain rejected before writing: invalid checksums, truncated metadata, out-of-file extents and unsupported descriptor versions do not become writable through a successful file checksum. Inspection is bounded to 256 volume-descriptor records, 4 MiB per inspected ISO directory and 4,096 El Torito catalog records. These limits keep untrusted metadata from demanding unlimited inspection work.

After updating Kavrune, close any old application process and reopen it to use the new inspector. For the full creation steps, see [Getting started](GETTING_STARTED.md).
