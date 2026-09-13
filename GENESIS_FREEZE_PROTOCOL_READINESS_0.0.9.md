# QRX 0.0.9 Genesis Freeze - Common Protocol Readiness Framework

Date: 2026-09-12

## Purpose

QRX Mainnet launches with the 0.0.9 wallet/core while major post-Genesis protocol layers remain fail-closed. Calendar dates are target/not-before policy dates only. Actual activation is scheduled by a threshold-signed on-chain `GOVERNANCE_PROTOCOL` transaction at a future block height after the corresponding readiness gate passes.

## State machine

`LOCKED -> NOT_READY -> SOAKING -> WAITING_TARGET_DATE -> READY_FOR_GOVERNANCE -> SCHEDULED -> ACTIVE`

Soak is a real-network burn-in/observation period measured in blocks. It prevents a transient provider spike from being mistaken for stable infrastructure.

## Staged activation plan

- 15 Sep 2026 - Genesis: Chain, QUB, Staking, Native Assets.
- 30 Nov 2026 - `DRIVE_V1`: Proof of Storage / QRX Drive.
- 07 Dec 2026 - `QRX_NET_V1`: Domains / Hosting / Browser.
- 15 Dec 2026 - `ADVERTISING_V1`: Advertising Economy.
- 31 Jan 2027 - `COMPUTE_POUC_V1`: Proof of Useful Compute / AURA rewards.

## Readiness policy

### DRIVE_V1
- 14 serving providers
- 14 independent operators
- 4 ASNs
- 3 visible regions
- 14 GiB proven capacity
- 80% average availability
- 80% proof success
- health score >= 60
- 7-day provider maturity soak

Preflight capacity/provider identity/attestation operations are allowed before activation. Economic storage contracts and rewards stay disabled.

### QRX_NET_V1
- DRIVE active
- 14 serving providers
- 4 ASNs / 3 regions
- 3 active storage contracts
- 85% availability
- health >= 70
- 7-day Drive dependency soak

### ADVERTISING_V1
- QRX-Net active
- 14 serving providers
- 4 ASNs / 3 regions
- 3 active `.qrx` domains
- 85% availability
- health >= 70
- 7-day QRX-Net dependency soak

### COMPUTE_POUC_V1
- DRIVE + QRX-Net active
- 8 on-chain compute-provider identities
- all nine existing PoUC safety/readiness evidence flags
- 14 storage providers
- 4 ASNs / 3 regions
- health >= 70
- 14-day dependency/shadow soak

Compute-provider identity binding is permitted during preflight; paid compute jobs, escrow settlement and rewards remain activation-gated.

## Consensus activation

Mainnet no longer treats local `protocol_upgrades.db` as authority. A 3-of-5-signed v2 governance proposal is broadcast as `GOVERNANCE_PROTOCOL`, atomically committed through QRXDB/WAL, replay-protected, and becomes common consensus state. At the scheduled height all conforming nodes enable the same feature.

## Wallet/RPC

`qrxd` exposes `getprotocolreadiness FEATURE_FLAG`. The GUI shows the four-stage roadmap and the current status, criteria progress, dependency progress, soak progress and blocking reason.

## Evidence

- Phase 181: DRIVE readiness preflight/maturity
- Phase 182: on-chain protocol governance; duplicate signer and local-file bypass checks
- Phase 183: shared readiness framework across DRIVE -> QRX-Net -> Advertising/Compute
- Full registered Core matrix: 116/116 PASS
- Targeted Phase 181/182/183 ASan+UBSan: PASS
- Wallet inline JavaScript syntax check: PASS
- Native Tauri/macOS build remains a native CI/host release check; Rust toolchain is not present in this Linux audit environment.

This is internal engineering evidence, not an independent third-party security certification.
