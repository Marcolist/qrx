# QRX 0.0.8.63 – Daemon Transfer Runtime

Implemented on top of 0.0.8.62.

## Core runtime
- Long-lived Drive transfer job manager owned by `qrxd`.
- Async upload/download jobs with IDs and snapshots.
- Pause / resume / cancel gates.
- Persistent v2 transfer journals.
- Restart recovery: unfinished jobs return as PAUSED and fail closed until verified provider discovery is restored.
- Per-range download progress, hedge/resume counters and shard progress.
- Client-side SHA3-256 verification against each authoritative per-shard CAS object ID before reconstruction.
- Atomic destination write via `.qrxpart.<transfer-id>` + rename.

## Provider lifecycle
- `QRXSTOR1` is accepted on the normal node listener when storage is enabled.
- The listener peeks the protocol magic and routes storage connections to the authorized storage handler.
- No separate central storage service is required.
- Provider PUT/GET remains contract/shard/provider/object-ID authorized.

## RPC / CLI
- `startdrivedownload <contract_id> <destination>`
- `startdriveupload <contract_id> <source>`
- `getdrivetransfer <transfer_id>`
- `listdrivetransfers`
- `pausedrivetransfer <transfer_id>`
- `resumedrivetransfer <transfer_id>`
- `canceldrivetransfer <transfer_id>`

Local file paths are hex-encoded between `qrx-cli` and `qrxd`, so whitespace in paths cannot corrupt the legacy whitespace command parser.

## Wallet
- Download now starts a daemon transfer instead of displaying a placeholder.
- Selected files can be uploaded against an existing authoritative storage contract.
- Live transfer polling displays bytes, shard progress, hedge count and resume count.
- Pause / Resume / Cancel controls are wired to daemon RPC.
- Authoritative shard routes remain connected to the privacy-safe Resource Globe.

## Tests
- `storage_phase104_drive_runtime`: real qrxp2p loopback; 14 shard upload and 10-of-14 download/reconstruction.
- `storage_phase105_drive_recovery`: persisted unfinished job is restored PAUSED and cannot resume without verified provider discovery.
- Full Core CTest: 38/38 PASS.
- Wallet inline JavaScript: `node --check` PASS.

## Deliberate remaining boundary
The daemon owns the discovery table and transfer runtime, but this snapshot still has no node-level signed provider-announcement gossip ingress that populates the daemon table from remote peers. The transfer start RPC therefore fails closed with `no currently verified provider discovery sources for contract` until authenticated discovery entries are supplied by that networking path. `quic://` also remains fail-closed; the working transport is real `qrxp2p://`, not TCP mislabeled as QUIC.

The current reconstruction path fetches bounded ranges and atomically streams the reconstructed result to a temporary destination file, but erasure reconstruction itself still materializes the reconstructed logical object in memory. Truly unbounded memory-constant Reed-Solomon stripe reconstruction remains a follow-up hardening block.
