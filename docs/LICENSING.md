# Licensing and project identity

Kavrune's implementation, documentation, stylesheet and SVG icon were created independently for this project. The application uses documented Linux storage and desktop interfaces. Its tests generate synthetic images from original fixtures; no proprietary operating-system files or bootloader binaries are bundled.

Original project source, documentation and artwork are licensed under the [MIT License](../LICENSE), copyright 2026 Kavrune contributors. Keep the license and copyright notice when distributing copies or substantial portions of the project. Contributions are accepted under the same license unless an explicitly reviewed third-party component is documented separately.

The application dynamically links system libraries and calls system services through their documented IPC interfaces. Those components keep their respective licenses; MIT licensing of the project does not relicense dependencies. See [third-party notices](../THIRD_PARTY_LICENSES.md).

**Kavrune** is the project and application name; `org.kavrune.Kavrune` is its application identifier. Names of third-party dependencies identify their respective components only and do not imply endorsement.

Name selection and an independent implementation are not legal clearance. No warranty of absence of trademark, patent, copyright or dependency-license issues is made. Distributors should check the names and rights relevant to their intended markets and artifacts. The MIT License does not provide a trademark-clearance guarantee.

Before each public release, record exact dependency versions and their licenses, include required notices/source/relink rights where applicable, review transitive dependencies, generate a complete SBOM and audit vulnerabilities. The included SBOM generator is a direct-system-dependency inventory, not proof of a completed compliance or security audit.

Contributions must state provenance, declare dependencies/assets, and avoid unlicensed external code. A distributor should review actual build inputs and shipped artifacts rather than relying only on this file. The [disclaimer](../DISCLAIMER.md) explains operational limits and does not add restrictions to the MIT License.

Kavrune is provided "as is", without warranty. To the fullest extent permitted by applicable law, its authors and copyright holders disclaim responsibility and liability for claims, damages or other liability arising from the software or its use. The canonical [MIT License](../LICENSE) contains the governing terms.
