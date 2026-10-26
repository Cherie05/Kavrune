# Privacy and legal implementation inventory

This inventory links actual notices and implementation. It is not a legal opinion, compliance certificate or trademark clearance.

| Requirement | Current implementation | Acceptance still needed |
|---|---|---|
| Original software license | [MIT](LICENSE), [NOTICE](NOTICE) and copyright; original code/artwork; no extra click-through terms restricting MIT rights | Review contribution/asset provenance for each release. |
| Terms and storage risk | [TERMS.md](TERMS.md), [DISCLAIMER.md](DISCLAIMER.md), exact target confirmation and irreversible-data warning | Legal review appropriate to actual publisher, distribution and markets. |
| Privacy behavior | [PRIVACY.md](PRIVACY.md); local files/device metadata, bounded XDG logs, user-directed copy/export/clear; no account, telemetry, automatic uploads or crash reporting | Verify future changes and dependencies; package-manager/OS services and network-mounted paths have independent behavior. |
| Offline access | Embedded legal reader in Help/About; packaged/root source notices | Package checker verifies copies and resources. No internet or consent needed to read. |
| Third-party notices | [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md), [licensing guide](docs/LICENSING.md), system-library dynamic linkage and direct-runtime SPDX inventory | Full transitive dependency/asset license and vulnerability review; complete release SBOM. |
| Support and disclosure | [SUPPORT.md](SUPPORT.md), [SECURITY.md](SECURITY.md), GitHub issue forms and enabled private vulnerability reporting in the official repository, verified 26 October 2026 | Maintain the hosted channel; no fallback email, conduct contact or support SLA is invented. Forks must configure their own reporting. |
| Data retention and removal | Manual image/backup ownership; bounded local diagnostics with export/clear; uninstall does not delete user files | Document each package manager's behavior and verify in clean VMs. Ordinary deletion is not forensic erasure. |
| Accessibility | Native controls and labels, keyboard routes, compact/theme/motion tests | Complete Orca/keyboard/contrast/scaling acceptance and jurisdiction/platform review. |
| Inapplicable policies | No refunds/subscription/cookie/account closure/OAuth/AI processing policies | Do not invent policies for absent services. Reassess only if product scope changes. |

Published terms do not exclude rights/liabilities that law does not allow excluding. India-specific or other market obligations depend on who distributes the application, actual processing and provisions in force; this project does not claim to have established legal applicability or compliance. Do not invent a legal entity, postal address, privacy officer or contractual SLA.
