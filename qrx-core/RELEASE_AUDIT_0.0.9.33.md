# QRX 0.0.9.33 Release Audit

Date: 2026-09-10
Release: `0.0.9.33 – Canonical Reorg Hook, Verification Reward Journal & Liveness Parameter Governance`

## Full-tree continuity

Baseline: canonical `qrx-core-0.0.9.32-verifier-timeout-reselection-reorg-penalty.zip`.

Before release-document additions, the 0.0.9.33 working tree compared with the 0.0.9.32 ZIP as follows:

- baseline regular files: **1065**
- removed baseline files: **0**
- new implementation files: **3**
- changed existing files: **5**

New implementation files:

- `qrx-core/src/compute/qrx_pouc_reorg.c`
- `qrx-core/src/compute/qrx_pouc_reorg.h`
- `qrx-core/tests/compute_phase158_canonical_reorg_reward_governance.c`

Changed implementation files:

- `qrx-core/CMakeLists.txt`
- `qrx-core/src/compute/qrx_pouc_liveness.c`
- `qrx-core/src/compute/qrx_pouc_liveness.h`
- `qrx-core/src/compute/qrx_pouc_verifier.c`
- `qrx-core/src/qrx.c`

No reduced/core-only packaging is used. The canonical ZIP is built from the complete project tree.

## Build validation

A fresh Release configuration was created with:

- `QRX_BUILD_TESTS=ON`
- `CMAKE_BUILD_TYPE=Release`
- `QRX_REQUIRE_PQC=OFF` for this container validation only

The first full build invocation reached approximately 89% before the execution environment time limit terminated it. The same clean build tree was then resumed and completed successfully. The `qrx` executable was linked successfully.

The source tree still contains pre-existing compiler warnings, primarily in the historical monolithic `qrx.c`; this audit does not claim a warning-free whole-project build. The newly added 0.0.9.33 compute module itself did not emit compiler warnings in the validation build.

Production/Mainnet PQC requirements are not changed by the container-only `QRX_REQUIRE_PQC=OFF` validation flag.

## Test validation

Full registered CTest suite from the fresh Release build:

**91/91 PASS**

New test:

`compute_phase158_canonical_reorg_reward_governance`

Coverage includes canonical liveness-parameter API validation, persisted verifier/challenger reward journal entries, QRXDB reopen recovery, descending-height canonical reward rollback, balance/pool/escrow restoration and idempotent repeat rollback.

## Governance integration validation

A separate real CLI integration was run on a temporary `qrx-regtest` chain:

1. initialized the chain,
2. generated three governance Ed25519 keypairs,
3. configured governance threshold 3-of-3,
4. created a `COMPUTE_LIVENESS_PARAMS` proposal,
5. signed with all three independent governance roots,
6. applied the proposal through `governance-apply`,
7. verified the resulting QRXDB keys and parameter-history record.

Result: **PASS**.

Verified values in the test were:

- `verifier_miss_threshold = 4`
- `verifier_miss_jail_blocks = 200`
- governance threshold = 3 unique valid signatures

## Canonical reorg integration

The chain block-ingest recovery path now checks whether a height is already indexed with a different block hash. Before replacing that height, it invokes `qrx_pouc_canonical_reorg_hook()` against the common parent (`height - 1`).

The PoUC coordinator restores, in order:

1. durable settlement journal,
2. liveness/reselection plus penalty state,
3. verification/challenge reward economics.

This is deliberately scoped to PoUC state. It does not falsely claim a universal undo journal for every unrelated QRX subsystem.

## Release tree and integrity

Final full-tree regular-file count, including release documentation and manifest: **1073**.

The release contains `RELEASE_MANIFEST_0.0.9.33_SHA256.txt`, generated over every regular release-tree file except the manifest itself. The packaged ZIP is separately hashed after ZIP creation to avoid a circular hash dependency.

## Result

Release packaging/audit status: **PASS**.

Next planned phase: **0.0.9.34 – PoUC Upstream State Undo Journal & End-to-End Reorg Atomicity**.
