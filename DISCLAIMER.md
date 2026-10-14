# Storage and project disclaimer

Kavrune 0.1 is a development preview. Physical USB qualification, the complete distribution/firmware test matrix and independent security review remain pending. It is not represented as a certified production or enterprise release. See [VALIDATION.md](docs/VALIDATION.md).

Writing an image, restoring a backup or formatting a drive permanently replaces data on the selected target. Keep an independent backup and check the model, capacity, serial and device path shown in confirmation. Device detection, confirmation and readback verification reduce risk but cannot guarantee protection against every failure, hardware defect or operating-system race. Cancellation, unplugging, power loss and errors can leave incomplete or unusable media. Raw writing is not secure erasure.

A checksum verifies byte consistency against a supplied value; it does not establish that an image is trustworthy, safe or bootable on your firmware. Obtain installation images from a source you trust. Use disposable media for preview evaluation and follow the documented safety limits.

Kavrune runs through system UDisks2 and Polkit authorization. It does not include telemetry or upload images, drive contents or logs. Local activity logs can contain device identifiers and paths; review and redact them before sharing.

Kavrune is an independently implemented project, with original project source and artwork. It is not affiliated with, endorsed by or an official Linux version of Rufus or any other USB utility. Third-party product names and components remain subject to their owners' rights. Choosing the Kavrune name is not a guarantee of trademark clearance.

Original project files are available under the [MIT License](LICENSE), including its warranty and liability terms. Dependencies retain their own licenses as described in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). This document explains use limitations; it does not add conditions or restrictions to the MIT License.
