# QRX 0.0.8.56 — Real Multi-Provider 10-of-14 Download

Status: implemented and regression-tested

This checkpoint continues the 0.0.8 performance roadmap after authorized P2P range serving and hierarchical large objects.

## Implemented
- Provider-aware shard sources with provider identity plus latency/throughput/reliability metrics.
- Real concurrent shard fetch workers using bounded range requests.
- STANDARD 10-of-14 reconstruction: return as soon as any 10 valid independent shards complete.
- Ranked initial fan-out and request hedging into additional providers.
- Resumable shard reads: short reads and bounded ranges continue from the last verified byte offset.
- Per-range retry budget.
- Optional complete-shard integrity callback before a shard is admitted.
- Cancellation signal after K successful shards; late workers are ignored/cancelled logically.
- Download statistics: started/completed/failed providers, successful shards, hedges, resumed ranges, cancelled sources and received bytes.
- Reed-Solomon reconstruction and join are wired directly into the fetch path.
- No whole-object RAM requirement is introduced by the transport API; fetches remain range bounded. The current reconstruction API operates one erasure stripe/object shard-set at a time.

## Safety properties
- Duplicate shard indices are rejected.
- Out-of-profile shard indices are rejected.
- A failed/slow provider cannot block reconstruction when 10 valid STANDARD shards are available.
- Corrupt shards can be rejected by the caller-provided verifier before reconstruction.
- The transport remains independent of a central coordinator. Provider discovery is intentionally the next layer.

## Verification
Developer build (QRX_REQUIRE_PQC=OFF because the container OpenSSL is not the production PQC toolchain):
- 31 / 31 tests PASS
- new `storage_phase098_multifetch` test covers:
  - 10+4 Reed-Solomon data
  - one dead provider
  - one deliberately slow provider
  - hedged provider launch
  - repeated short range reads / resume
  - complete-shard verification
  - successful byte-for-byte reconstruction

Production release validation must still be repeated with QRX_REQUIRE_PQC=ON and the project's OpenSSL >= 3.5 PQC build.

## Next roadmap blocks
1. Provider Discovery over QRX P2P/DHT/Gossip, producing authenticated fetch candidates without a central coordinator.
2. Drive GUI foundation wired to real storage state and transfer progress.
3. QRX-Net browser/resource state integration.
4. Resource Globe GUI foundation with privacy-cell aggregation.
