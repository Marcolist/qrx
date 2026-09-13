# QRX 0.0.8.54 Core Foundation Checkpoint

Date: 2026-09-08

This checkpoint is a full source-tree continuation from the last complete 0.0.8.2 source archive. It deliberately does not move QRX Drive/QRX-Net activation into Genesis. Mainnet activation remains a post-Genesis mandatory protocol upgrade through the existing threshold-signed upgrade schedule.

## Newly implemented in this checkpoint

- Fixed internal release-test harness so assertions remain active in RelWithDebInfo tests (`-UNDEBUG`).
- Fixed the filesystem CTest command so it executes only its intended test binary.
- SHA3-512 Merkle tree/proof implementation with domain-separated leaf/node hashing.
- Delayed finalized-block Capacity Proof challenge and verification.
- `qrx-drive-pq-v1` data-at-rest crypto:
  - AES-256-GCM file encryption
  - X25519MLKEM768 hybrid KEM key envelope
  - HKDF/SHA3-256 derived wrapping key
- ML-DSA-65 signed canonical Drive manifests.
- Reed-Solomon erasure coding with systematic generator matrix:
  - STANDARD 10+4
  - ARCHIVE 16+4
  - arbitrary recoverable shard-loss reconstruction tested
- Failure-domain-aware deterministic provider placement.
- Deterministic Merkle PoStor challenge/verification.
- Provider reputation, capped performance factor, capacity-scaled bond, exit delay, slashing primitives.
- Storage economic split and provider/repair/egress accounting primitives.
- Autonomous repair planning and replacement-provider lifecycle primitives.
- Privacy-aggregated Resource Atlas, network health, dynamic demand, Opportunity and Storage Mission primitives.
- Transport-neutral shard fetch planning with k+hedge and cancel-after-k.
- Canonical resumable shard range wire format.
- QRX-Net `.qrx` canonical naming rules and SHA3 name hashes.
- Domain economics helpers including scarce-name auction mode, portfolio reservation bond and 0.5% development split.
- PQ publishing-key commitment for domain ownership/content authorization.
- Resolver routing that never allows `.qrx` names to fall through to normal DNS, even if entered as `http://` or `https://`.
- Persistent atomic QRXDB Name Registry:
  - REGISTER
  - RENEW
  - UPDATE with KEEP/SET/CLEAR semantics
  - TRANSFER
  - history snapshots per sequence
  - transfer clears mutable resolver/publishing records
- PUBLIC_SIGNED QRX-Net website bundles stored through native QRX Drive CAS.
- Per-file SHA3-512 and Merkle integrity, canonical bundle root, ML-DSA-65 site signatures.
- Safe DApp permission model with only:
  - PUBLIC_ADDRESS
  - REQUEST_SIGNATURE
  - REQUEST_PAYMENT
  - PUBLIC_DRIVE_READ
  - NOTIFICATIONS
- DApp grants bind to current domain owner and PQ publishing-key commitment; owner transfer/key rotation invalidates the grant.
- Atomic local persistence format for DApp grants.

## Validation

- Core internal tests: 25/25 PASS with `QRX_REQUIRE_PQC=ON` and RelWithDebInfo.
- Existing VELOCITY/MVCC/SPV regression tests remain PASS.
- I18N audit: 55 locales, 826 keys each, PASS.
- I18N quality gate: PASS.
- I18N lexical audit: PASS.

## Not yet claimed complete

The full 0.0.8.x roadmap is not yet complete. In particular the following still require further implementation/integration and validation:

- Consensus `applytx` wiring for all storage/provider/settlement/repair/domain transaction types.
- Full decentralized network/failure-domain attestation consensus flow.
- Real socket/QUIC transport backend and P2P shard discovery/serving.
- Full end-to-end storage contract assignment -> PoStor -> payout state in QRXDB.
- QRX Drive/Provider GUI and the 3D Resource Globe UI.
- QRX Browser native Tauri remote-WebView sandbox build and compile verification.
- Site publisher directory workflow and browser offline cache UX.
- QRXScan/QRX OS integration.
- New UI translation keys across all locales.
- Adversarial storage/QRX-Net testnet suite.
- Final Genesis-equivalence audit against the release Genesis source.
- DRIVE_V1 + QRX_NET_V1 Mainnet readiness gate.

