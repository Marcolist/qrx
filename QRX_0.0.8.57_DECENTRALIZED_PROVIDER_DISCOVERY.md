# QRX 0.0.8.57 — Decentralized Storage Provider Discovery

Implementation snapshot following 0.0.8.56 Multi-Provider Download.

## Added

- `qrx_storage_discovery.[ch]` as the storage/resource discovery cache and validation layer.
- Signed provider announcements with canonical `qrx-storage-discovery-v1` domain separation and SHA3-512 commitments.
- Crypto-agile EVP signing/verification: the discovery wire object is not coupled to one signature algorithm; current QRX ML-DSA keys are supported.
- Monotonic per-provider announcement sequence numbers for replay/stale-gossip rejection.
- Height-bounded announcement lifetime (`valid_from_height` / `valid_until_height`).
- Capability advertisement for range reads, resume, QUIC, proof serving and public-content serving.
- P2P transport endpoint advertisement restricted to `quic://` and `qrxp2p://` in v1; browser/DNS URLs are rejected from this layer.
- Discovery ingest is fail-closed against QRX chain state: provider must exist, be ACTIVE and have non-zero proven capacity.
- Provider ID -> public-key lookup callback binds network announcements to the authoritative provider identity/key source used by the node.
- Gossip table merge with full re-verification; a peer cannot make unsigned/stale entries trusted merely by forwarding them.
- Expiry pruning.
- Contract source resolution: discovery candidates are intersected with authoritative active on-chain shard assignments before they become downloader sources.
- Provider metrics are converted to `QrxShardProviderSource` and ranked through the existing fetch-plan logic for 10-of-14 fetching.

## Security properties

- No central discovery server or coordinator.
- Bootstrap peers can introduce peers but cannot authorize storage access.
- A forged announcement signed by another provider key is rejected.
- Same-sequence/stale announcement replay is rejected.
- Expired endpoint announcements are rejected/pruned.
- Unregistered, inactive or capacity-unproven providers cannot enter the trusted discovery table.
- Discovery does not override contract placement: chain assignments remain authoritative.
- Every gossip merge re-runs chain and signature verification.

## Validation

New regression test: `storage_phase099_discovery`

Covers:
- 14 active/proven providers
- signed announcement ingest
- gossip merge
- anti-replay
- wrong-key forgery
- newer sequence replacement
- authoritative 14-shard contract source resolution
- expiry/pruning

Full CTest result: **32/32 passed**.

## Next implementation block

Drive / QRX-Net / Resource-Globe GUI foundation should consume aggregated resource/network state rather than raw home-node endpoints. Exact endpoints remain internal to P2P transport; Globe/UI data must follow the privacy-cell aggregation rules from the roadmap.
