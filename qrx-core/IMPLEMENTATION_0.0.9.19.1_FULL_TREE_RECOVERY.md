# QRX 0.0.9.19.1 – Full Tree Recovery & Merge Audit

Date: 2026-09-09

## Reason
Release packaging regressed after 0.0.9.10. The 0.0.9.10 archive contained the full QRX project tree, while 0.0.9.11–0.0.9.19 packaged only the flattened `qrx-core` subtree. The code work from 0.0.9.11 onward was therefore not necessarily lost, but the distributed release archives no longer contained the complete project snapshot.

## Recovery procedure
1. Used `qrx-core-0.0.9.10-aura-chat.zip` as the last known complete project snapshot.
2. Extracted `qrx-core-0.0.9.19-native-runtime-calibration.zip`.
3. Recognized the structural difference: 0.0.9.10 stores the core in `qrx-core/`; 0.0.9.19 is that subtree flattened beneath `qrx_0919_release/`.
4. Overlaid the complete cumulative 0.0.9.19 core subtree onto `0.0.9.10/qrx-core/`.
5. Verified every regular file from the 0.0.9.19 subtree exists byte-for-byte identically in the recovered `qrx-core/` subtree.
6. Preserved the complete non-core project tree from 0.0.9.10, including GUI wallet and project-level scripts/docs.
7. Restored the current canonical 0.0.8–0.0.10 roadmap in the recovered release.

## File audit
- 0.0.9.10 complete snapshot: 976 regular files, 36,128,173 bytes uncompressed.
- 0.0.9.19 reduced snapshot: 394 regular files, 2,913,249 bytes uncompressed.
- Recovered project before this audit note: 995 regular files, 36,383,576 bytes uncompressed.
- Latest-core overlay verification: 0 missing files, 0 byte differences.

The recovered tree intentionally preserves the complete 0.0.9.10 outer project while replacing/updating `qrx-core/` with the cumulative 0.0.9.19 state.

## Build and tests
A fresh Linux CMake release build was performed on the recovered `qrx-core` tree with internal tests enabled.

Full registered CTest suite result:
- 77/77 tests PASS
- includes storage, QRX-Net, VELOCITY, Bitcoin SPV and compute phases 126–144
- compute phases 134–144: 11/11 PASS

The build still emits pre-existing compiler warnings in older source files (for example misleading indentation / possible snprintf truncation). This recovery patch does not claim those warnings have been eliminated.

## Packaging rule going forward
All subsequent QRX 0.0.9 release archives must be produced from the complete project tree. A reduced/flattened `qrx-core` subtree may be used as a development delta, but must not be distributed as the canonical full release archive unless explicitly labelled as a core-only/delta package.
