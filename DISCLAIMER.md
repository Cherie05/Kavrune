# Storage, warranty and liability notice

## No warranty or liability

Kavrune is provided "as is", without warranties or guarantees of any kind, as stated in the [MIT License](LICENSE). There is no warranty of merchantability, fitness for a particular purpose, reliability, uninterrupted operation or freedom from errors.

To the fullest extent permitted by applicable law, the authors and copyright holders are not responsible or liable for any claims, damages, losses or other liability arising from Kavrune or its use, including lost or corrupted data, unusable media, device or system damage, interrupted work or lost profits. Users are responsible for selecting the intended target drive and keeping independent backups.

The MIT License contains the governing warranty disclaimer and limitation of liability. This notice explains those terms and operational limits; it does not add conditions or restrictions to the license or exclude rights and liabilities that applicable law does not permit to be excluded.

## Storage and development limits

Kavrune 0.1 is a development preview. Physical USB qualification, the complete distribution/firmware test matrix and independent security review remain pending. It is not represented as a certified production or enterprise release. See [VALIDATION.md](docs/VALIDATION.md).

Writing an image, restoring a backup or formatting a drive permanently replaces data on the selected target. Keep an independent backup and check the model, capacity, serial and device path shown in confirmation. Device detection, confirmation and readback verification reduce risk but cannot guarantee protection against every failure, hardware defect or operating-system race. Cancellation, unplugging, power loss and errors can leave incomplete or unusable media. Raw writing is not secure erasure.

A checksum verifies byte consistency against a supplied value; it does not establish that an image is trustworthy, safe or bootable on your firmware. Obtain installation images from a source you trust. Use disposable media for preview evaluation and follow the documented safety limits.

Kavrune runs through system UDisks2 and Polkit authorization. It does not include telemetry or upload images, drive contents or logs. Local activity logs can contain device identifiers and paths; review and redact them before sharing.

## Independent project and dependencies

Kavrune has independently written project source and original artwork. Third-party libraries and system services retain their owners' rights and licenses. Choosing the Kavrune name is not a guarantee of trademark clearance.

Original project files are available under the [MIT License](LICENSE), including its warranty and liability terms. Dependencies retain their own licenses as described in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). This document explains use limitations; it does not add conditions or restrictions to the MIT License.
