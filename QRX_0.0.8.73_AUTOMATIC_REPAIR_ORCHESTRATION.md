# QRX 0.0.8.73 — Automatic Repair Orchestration

Status: DONE

- permissionless decentralized repair-start scanning after missed PoStor window
- deterministic replacement provider remains consensus-selected from finalized randomness and existing failure-domain exclusions
- replacement provider reconstructs STANDARD missing shard directly from healthy ACTIVE peers; owner need not be online
- new bounded-memory stripe reconstruction for exactly one erasure shard
- every selected source shard is SHA3-256 CAS verified after streaming
- reconstructed target is accepted only if its SHA3-256 CAS equals the original assignment object_id
- repair PUT authorization is moved from the old provider to the consensus-selected replacement provider
- original CAS / physical bytes / PoStor Merkle root / leaf count remain invariant across repair
- replacement provider automatically submits STORAGE_REPAIR_ACCEPT after exact local object is present
- repair record is restart-readable through qrx_storage_repair_get
- first replacement PoStor is mandatory before STORAGE_REPAIR_COMPLETE
- repair-complete proof is fixed to epoch 1 and challenge_height = repair_accept_height + proof window
- finalized block hash is mandatory for replacement proof randomness
- provider signs repair transactions with existing Ed25519 + ML-DSA wallet identity
- persistent 12-block markers suppress duplicate START / ACCEPT / COMPLETE submissions across restart
- node runtime executes repair scans during normal operation and after storage connections
- regression: storage_phase114_repair_runtime
- full Core CTest: 47/47 PASS

Next: 0.0.8.74 — QRX-Net Domain Management + PQ Ownership Wiring
