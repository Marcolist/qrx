QRX 0.0.8.64 – Signed Discovery Gossip, Authenticated Cache & Streaming Reconstruction
===================================================================================

Implemented on top of 0.0.8.63.

1. Signed provider-discovery gossip envelope
- QRXDISC1 bounded binary envelope.
- Carries the exact canonical provider announcement plus provider signature.
- Decode alone never grants trust.
- Every live/cache entry must pass qrx_storage_discovery_ingest():
  active chain provider, non-zero proven capacity, provider key lookup,
  signature verification, validity height and monotonic sequence/replay checks.

2. Persistent authenticated discovery cache
- QRXDC01 cache format.
- Atomic temp-file save/rename.
- Signatures are persisted with announcements.
- Cache load replays every record through the same cryptographic/chain-state
  ingestion path used for live gossip.
- Corrupted/tampered signatures are rejected instead of trusted after restart.

3. Memory-constant erasure restore
- qrx_storage_stream_reconstruct_to_file().
- Bounded stripe reads (default 128 KiB).
- A stable 10-of-14 provider set is selected for a restore stripe stream.
- Reed-Solomon reconstruction is performed stripe-by-stripe.
- Data-shard stripes are written directly to their final file offsets.
- Peak erasure payload memory is O((data+parity)*stripe_size), not O(file_size).
- Every selected source shard is incrementally SHA3-256 hashed and checked
  against its authoritative per-shard CAS object ID before success.
- Partial output is deleted on integrity/fetch/reconstruction failure.

4. Daemon transfer runtime
- Download runtime now uses the streaming reconstruction path and writes to
  <target>.qrxpart.<transfer-id>, then atomically renames on success.
- Pause/cancel gating remains between provider range requests.
- Existing upload fan-out remains unchanged in this phase.

5. Regression coverage
- New storage_phase106_gossip_streaming test:
  signed wire roundtrip, authenticated cache restart, tampered-cache rejection,
  8 MiB+ streamed 10+4 restore with one unavailable provider and exact output.
- Existing runtime test adjusted to the real 10-of-14 read count.
- Full internal CTest: 39/39 passed.

Important remaining boundary
----------------------------
0.0.8.64 implements the authenticated gossip wire/cache primitives and the
runtime-safe ingestion boundary, but the legacy node P2P command dispatcher
still needs a canonical provider public-key source before QRXDISC1 can be wired
as an automatic network message in qrxd without weakening authentication.
Do not accept RPC-supplied arbitrary public keys/endpoints as a shortcut.

Recommended 0.0.8.65:
- consensus/provider identity key registry binding,
- qrxd STORAGE_DISCOVERY_PUSH / STORAGE_DISCOVERY_PULL gossip handlers,
- peer fan-out/dedup/rate limits,
- automatic cache load/save/prune in node lifecycle,
- discovery -> QrxDriveRuntime source refresh,
- replace qrx_storage_network tmpnam() upload staging with mkstemp/GetTempFileName.
