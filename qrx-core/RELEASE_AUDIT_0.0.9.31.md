# QRX 0.0.9.31 — Release Packaging / Audit

Date: 2026-09-10
Baseline: `qrx-core-0.0.9.30-pouc-upstream-consensus-transactions.zip`

## Full-tree continuity audit

Before adding the 0.0.9.31 release documentation, the current tree was compared file-by-file with the complete 0.0.9.30 archive while excluding build directories.

Result:
- 0.0.9.30 baseline files: 1036
- 0.0.9.31 working-tree files at comparison point: 1039
- added implementation files: 3
- removed baseline files: 0
- changed existing files: 11

Added code/test files:
- `qrx-core/src/compute/qrx_pouc_verifier.h`
- `qrx-core/src/compute/qrx_pouc_verifier.c`
- `qrx-core/tests/compute_phase156_verifier_selection_rewards_slashing.c`

The 11 changed files are limited to the expected CMake, PoUC compute/consensus/journal/pipeline/Mainnet integration, staking/validator integration and prior PoUC regression tests. No 0.0.9.30 file disappeared from the full project tree.

## Build/test audit

The existing assert-enabled 0.0.9.31 build was rebuilt against the final source tree and the complete registered CTest suite was rerun:

- registered tests: 89
- passed: 89
- failed: 0
- `compute_phase156_verifier_selection_rewards_slashing`: PASS

No compiler warning in that incremental audit build was attributed to the new `qrx_pouc_verifier.c` or Phase 156 test.

A separate clean optimized `Release` build was also attempted in the execution environment. It was terminated by the tool execution-time ceiling while compiling/optimizing the pre-existing monolithic `src/qrx.c`; the log did not show a 0.0.9.31 compile error before termination. Therefore this audit does **not** claim a freshly completed optimized binary Release build. The canonical artifact produced here is the full source/project release tree.

## Consensus/economic audit notes

Verified in source/test coverage:
- verifier identities are selected deterministically and are not self-selected by the transaction sender,
- owner/provider exclusion is enforced,
- verification rewards are drawn only from the user's explicit verification budget,
- unused verification budget returns through settlement/refund,
- FastTrack and normal protocol/staking economics are not replaced by the verifier reward layer,
- slash/jail execution requires explicit canonical parameters; there is no hidden default slash percentage,
- slashed stake enters a protocol slashing pool and is included in supply accounting,
- compute jail is consulted by validator eligibility paths,
- duplicate penalty application for the same receipt is rejected.

## Packaging rules

Canonical 0.0.9.31 packaging:
- full QRX project tree, not a core-only delta,
- no `build*` directory,
- no `.git` metadata,
- no temporary release-build directory,
- archive integrity tested after creation,
- SHA-256 manifest generated beside the ZIP.

Known next hardening target:
`0.0.9.32 — Verifier Timeout/Reselection & Reorg-Safe Penalty Journal`.
