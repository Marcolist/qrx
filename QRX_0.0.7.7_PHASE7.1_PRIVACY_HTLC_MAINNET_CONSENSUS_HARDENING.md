# QRX 0.0.7.7 Phase 7.1 — Privacy & HTLC Mainnet Consensus Hardening

This source archive contains the Phase 7.1 hardening work on top of QRX 0.0.7.7 Phase 7.

Implemented in this phase:
- Mainnet hard-disable for legacy file-backed Privacy and legacy HTLC paths.
- Signed consensus transaction types: PRIVACY_SHIELD, PRIVACY_TRANSFER, PRIVACY_UNSHIELD.
- QRXDB/WAL atomic application of transparent balance, privacy state, fee, lane nonce and applied-tx state.
- Verified-Privacy credential enforcement in consensus.
- Privacy governance migration to signed PRIVACY_GOVERNANCE consensus state.
- Privacy proof transcript binding to chain_id, genesis_hash and protocol_version.
- GUI migration from legacy Privacy/Quantum-Swap paths to signed Privacy transactions and VELOCITY Cross-Chain HTLC.
- BTC funding deadline/safety-window enforcement and immutable first funding proof/TXID.
- Bitcoin SPV hardening tests including mainnet genesis reference, retarget clamps, MTP/difficulty/parent rejection and deterministic malformed-header cases.

Validation recorded in this tree includes:
- Fresh PQC-enabled Core/qrxd build PASS.
- Velocity/MVCC/SPV CTest suite: 7/7 PASS.
- Privacy governance E2E PASS.
- Privacy SHIELD -> TRANSFER -> UNSHIELD consensus lifecycle PASS.
- Legacy Privacy regression tests PASS after chain/genesis-binding update.
- GUI JavaScript syntax/wiring checks PASS.

Release note:
- Native Tauri installers were not built in the audit environment because Rust/Cargo was unavailable there.
- An independent cryptographic audit is still required before treating the custom shielded-privacy construction as approved for real-value Mainnet use.
- The broader QRX Mainnet release remains gated by the separate Genesis/bootstrap/validator/seed-node release blockers identified in the Mainnet RC audit.
