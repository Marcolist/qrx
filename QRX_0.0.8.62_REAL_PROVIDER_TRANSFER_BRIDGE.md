# QRX 0.0.8.62 – Real Provider Transfer Bridge

Status: implemented in core transport foundation.

Implemented:
- real loopback/network `qrxp2p://` provider transport over sockets
- provider-side authorized PUT and bounded range GET
- contract/shard/provider/object-id authorization before reads/writes
- SHA3-CAS verification of uploaded shard data before acceptance
- per-shard object IDs carried by `QrxShardProviderSource` (fixes the prior single-object-id abstraction)
- parallel multi-provider upload helper (`qrx_storage_network_upload_many`)
- adapter-compatible fetch callback for existing 10-of-14 hedged/resumable downloader
- persistent atomic transfer journal for crash/resume state
- dedicated real-socket regression test

Security / correctness:
- discovery endpoint scheme is not silently reinterpreted
- `qrxp2p://` is implemented as the current real provider socket transport
- `quic://` remains fail-closed because this source tree does not yet link a real QUIC implementation
- no fake successful transfer status is exposed for unsupported QUIC endpoints
- individual erasure shards use their own authoritative CAS object IDs

Validation:
- Core build successful with developer PQC override in this environment
- 36/36 CTest tests passed
- new test: `storage_phase103_network_transfer`

Still required before calling the whole wallet flow production-ready:
- daemon-owned live discovery table populated from P2P gossip at runtime
- daemon provider listener lifecycle/configuration
- RPC commands that create and resume transfer jobs from Wallet UX
- streaming restore to destination file rather than final reconstructed buffer
- a genuine QUIC backend (e.g. ngtcp2/quiche/msquic or another audited cross-platform choice)
- end-to-end 14-provider integration test across processes/hosts
