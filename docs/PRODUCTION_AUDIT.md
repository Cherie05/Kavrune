# Kavrune production audit — 18 October 2026

**Decision: development preview; not yet qualified for general production or enterprise deployment.** The notices and verified fixes below improve the current application. They do not complete the hardware, firmware, distribution, independent security or legal acceptance work required for a supported release.

## Scope and evidence

This source and functional review covers the desktop workflows, image/parser and copy engine, storage authorization and target protection, local logging, backup handling, privacy/terms, repository/CI configuration and Debian packaging. Automated storage tests use private temporary regular files; UI tests use synthetic data on a hidden or private display. The audit does not write, format, unmount or otherwise change attached drives, or interrupt the user's running app.

The application source was checked for network uploads/downloads, shell execution, root bypasses, unbounded image reads, file/descriptor identity changes, cancellation, unsafe log paths and backup cleanup. This is an agent-assisted source review and regression exercise, not an independent security certification, comprehensive fuzzing campaign or completed dependency-vulnerability audit.

The earlier user-initiated Alpine write is recorded separately in [BUILD_RESULTS.md](BUILD_RESULTS.md). The current Mint ISO was structurally accepted and its checksum matched the publisher during test preparation; that is not a recorded Mint write or boot qualification result. User descriptions of boot screens are not independently witnessed firmware tests.

## Findings addressed

| Finding | Resolution and evidence |
|---|---|
| Terms and Privacy were absent; the storage disclaimer was not available from the app | Add [TERMS.md](../TERMS.md) and [PRIVACY.md](../PRIVACY.md), retain the [MIT License](../LICENSE) and [disclaimer](../DISCLAIMER.md), and embed all four documents in a searchable offline reader accessible from How to use and About. No forced consent or extra MIT conditions. |
| An event-log FIFO could block the UI before file-type validation | Open the optional log without waiting, then retain regular-file, owner, link and permission checks. Regressions cover FIFOs with and without a reader, and recovery. |
| Another writer holding the event-log lock could freeze startup or operation completion | Acquire the optional log lock without waiting; report logging failure separately from storage results. A separate-descriptor contention/recovery regression exercises the actual lock behavior. |
| Moving/replacing a backup file or its containing folder during saving could report success for an incorrect chosen path | Revalidate the chosen folder identity and created file before and after synchronization, and preserve replacement files during cleanup through the pinned directory descriptor. Temporary-file regressions simulate both file and folder replacement during saving. |
| Packaging had no repeatable extraction check for current notices or embedded legal text | Add [check-package.py](../scripts/check-package.py), run it before accepting newly generated packages, and wire it into CI. It checks source/package/resource equality, metadata, permissions, runtime search paths and PIE/RELRO/NX hardening; its optional CLI test uses disposable files. |

## UI review

The existing Create, Format, Backup, Check download and Check USB control tests verify explicit target selection, synchronized drive details, busy-state gating, invalid source/name handling and progress completion. The legal reader adds document switching, case-insensitive search, next/previous matches, empty/no-match feedback, selectable wrapped text, Ctrl+F, Escape and closing while an operation remains active. Its fixed embedded UTF-8 documents are displayed as text and do not execute markup or open a browser.

Private light/dark visual checks cover wide and compact legal-reader layouts; the actual About link was exercised without discovery or a storage operation. This is not a complete Orca, keyboard-only, high-contrast, scaled-text or translation certification. The full file chooser and every actual authorization/error interaction still require manual release validation.

## Release blockers and acceptance

| Area | Required evidence before qualification |
|---|---|
| Storage and recovery | Disposable hardware/VM matrix for authorization approval/denial/cancellation, device disconnect/replacement at each stage, I/O failures, interrupted formatting, backup/restore and system-disk protection across common storage layouts. One logged successful write does not complete this matrix. |
| Boot compatibility | Recorded checksums and results for representative Linux images on BIOS, UEFI and actual Secure Boot hardware. A parser's bootable flag and file-copy verification cannot establish firmware compatibility. |
| Raw target preparation | Define and test stale secondary-GPT/filesystem signature handling and data beyond the incoming image. Current raw writing replaces the image-length prefix and is not secure erasure. |
| Distribution and packaging | Clean install/launch/remove tests and compatible runtime dependencies for each claimed distribution/architecture; build on the oldest supported distribution. The local package is built on Ubuntu 26.04 x86_64, not a demonstrated universal Linux artifact. |
| Security and supply chain | Independent parser/storage review, broader adversarial testing, current direct/transitive dependency vulnerability and license review, complete SBOM, release provenance/signing and reproducibility. The generated SPDX file is only a direct runtime inventory. |
| Privacy, support and legal operations | Publish a functioning private security/privacy contact, decide retention/support commitments and review applicability in intended markets. Local owner-only logs are unencrypted and have no expiry/rotation or opt-out. No project name or legal clearance is implied by these notices. |
| Accessibility and enterprise support | Complete assistive-technology, keyboard, contrast/scaling and translation review; define supported versions, update practices and organizational acceptance/support responsibilities. There is no existing response-time SLA. |
| Hosted CI | Run and review the exact final published commit. A prior green baseline or local run does not verify later changes or hosted repository settings. |

Windows installer extraction, persistence, compressed-image streaming and fake-capacity testing remain outside the implemented preview scope. A future Linux raw-writer release must state its supported scope explicitly; adding unrelated features is not a substitute for qualifying that scope.

The final audit build passed all seven normal and all seven sanitizer suites: **759 assertions** across core, storage, GTK selection and desktop workflows. Package extraction and its temporary-file CLI self-test passed. A deliberately incomplete disposable package was rejected, and the original synthetic BIOS file-image test passed. These establish the exercised paths, not the outstanding acceptance matrix.

The detailed release checklist is [VALIDATION.md](VALIDATION.md), with a user-facing [USB trial checklist](USER_TEST_CHECKLIST.md). Current execution and artifact results are recorded in [BUILD_RESULTS.md](BUILD_RESULTS.md).
