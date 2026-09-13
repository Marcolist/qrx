# QRX 0.0.8.65 – Provider Identity + Live Discovery Gossip

Implemented:

- consensus-bound `STORAGE_PROVIDER_BIND_DISCOVERY_KEY`
- ML-DSA-65-only provider discovery identity lookup
- live `STORAGE_DISCOVERY_PUSH` / `STORAGE_DISCOVERY_PULL` node messages
- QRXDISC1 ingestion through the same authenticated discovery validator
- replay/sequence/height/provider/proven-capacity/signature validation
- bounded fan-out and existing peer rate limiting
- shared `chain_dir/storage-discovery.cache`
- node lifecycle load/save/prune
- qrxd reload/revalidation before transfer source resolution
- secure temporary provider upload staging (`mkstemp` / Windows secure temp API)
- regression test `storage_phase107_identity_gossip`
- 40/40 Core tests passing

## PQ storage status

The `qrx-drive-pq-v1` primitives are real and tested: AES-256-GCM chunks, a random 256-bit file key, X25519MLKEM768 wrapping, SHA3 content addressing, and ML-DSA-65 manifest support. The hierarchical large-object layer already uses these primitives.

The newer daemon transfer-job path is not yet automatically using that object layer for every upload; its current `upload_thread()` still erasure-encodes source bytes directly. Therefore the Wallet transfer path must not yet claim mandatory PRIVATE_PQ end-to-end encryption. 0.0.8.66 is explicitly reserved to connect the existing PQ object layer to the real multi-provider transfer runtime and make fail-closed PRIVATE_PQ the default for private Drive data.
