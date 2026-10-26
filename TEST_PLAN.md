# Kavrune test plan

## Rules and evidence

Automated writes use generated temporary regular files, never attached drives. Forced private X11/DBus is required for UI automation so a developer's app/display is untouched. Use synthetic filenames/data and private state directories. A skip is not a pass; record missing prerequisites. Tests must demonstrate behavior/failure handling rather than mirror implementation.

| Area | Measurable acceptance | Execution |
|---|---|---|
| Core/parser | Valid profile acceptance and bounded rejection; malformed offsets, CRCs/catalogs, mutation, symlinks, alignment, verification failures and cancellation | `scripts/build.sh`, core/storage CTest; repeat with ASAN/UBSAN. |
| Outputs and diagnostics | Refuse existing/replaced outputs; private permissions; bounded records/current/previous history; read/clear/export; unsafe links/types and competing locks fail promptly | Core temporary-file diagnostics/backup regressions; CLI diagnostics checks. |
| Native UI | Explicit USB choice shared across tasks; validation/busy gating; progress below completion until return; legal and Activity controls work; accessible names/labels | Actual GTK control tests and private compact/light/dark/motion visuals. |
| Shutdown during discovery | Closing the last window keeps the application loop running until successful or failed discovery finishes, without accessing destroyed widgets | GTK workflow tests use a gated synthetic query and the actual application loop; no physical drive discovery is required. |
| Offline artifact | Local self-test and private UI start with internet socket creation denied, while local IPC stays available | `scripts/check-offline.py`; executed on extracted package. Unsupported guards must skip or fail explicitly. |
| Packaging | Current notices/resources; no unsafe links/permissions/privileged bits or developer RPATH; hardened ELF; version consistency; self-test and disposable payload lifecycle | `scripts/package.sh` and `scripts/check-package.py`. Extraction/overlay/removal is distinct from real package-manager installation. |
| PPA source | Export committed files only, refuse dirty/source-link/existing output, preserve license, validate public metadata, maintain preview version ordering and round-trip a real source package | `python3 tests/test_ppa_packaging.py`; Ubuntu 24.04 CI builds the generated source through debhelper and checks its binary/offline payload. Signing/upload/real APT installation remain separate. |
| Performance | Provisional local UI smoke completes within 10 seconds including its deliberate 2-second delay, peak RSS below 512 MiB; local safe-test bounded runtime | Offline report records host measurements. UI event latency <100 ms, settled idle CPU <1%, cold useful UI <3 seconds and low-memory behavior are targets requiring baseline hardware measurements, not inferred from smoke. |
| Boot | Original 4 MiB synthetic BIOS fixture emits exact marker with unchanged backing file | Optional `scripts/test-boot.sh`; distribution/UEFI/Secure Boot remain separate. |
| Source release | Policies/standards/docs present, no generated/private artifacts, GitHub noreply author and committer throughout complete HEAD ancestry, matching source/artifact fingerprint and checksums | `scripts/check-repository.py --staged --history`; `python3 tests/test_repository_privacy.py` uses disposable Git histories; final manifest/checksums. Hosted legacy PR/cache refs and build records need separate review. |

## Required manual/disposable environment cases

Use [VALIDATION.md](docs/VALIDATION.md) and [USER_TEST_CHECKLIST.md](docs/USER_TEST_CHECKLIST.md). Record exact commit, distro, image hash, target hardware/firmware, steps, logs and result. Cover authorization approve/deny/cancel/no agent, missing UDisks/tools, full destination, protected root, home, swap, RAID, LUKS, LVM, Btrfs and overlay storage, device removal/replacement at each stage, cancelled/failed backup restore, partial formatting and stale-tail signatures.

Test a supported package in clean online and offline VMs (dependencies preinstalled for offline), reinstall, upgrade, removal and user-data retention. Test real USB 2/3 controllers, sector sizes, multiple monitors, 420/800px layouts, 200% text/scaling, RTL expansion, high contrast, keyboard-only and Orca. Source files with Unicode/spaces, very long paths, permissions and changed parent directories must receive safe results.

Run air-gapped local-file operations independently of network-mounted paths. Do not disconnect the host's real network for automation. Process-local socket denial is limited evidence, not a claim that the entire OS is offline. Monitor memory/CPU during real long operations and explicit suspend/logout requests on disposable media. Never force host shutdown or kill a user's storage job to satisfy a test.
