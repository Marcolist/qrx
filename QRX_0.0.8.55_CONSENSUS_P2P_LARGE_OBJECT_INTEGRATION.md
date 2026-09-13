# QRX 0.0.8.55 — Consensus, P2P & Large Object Integration

Status: implementation checkpoint

## Mandatory post-genesis upgrade
- Genesis remains on the stable 0.0.7.7 consensus.
- DRIVE_V1 and QRX_NET_V1 remain post-genesis mandatory Protocol 9 feature gates.
- Mainnet fails closed until the signed activation schedule reaches its activation height.

## Atomic consensus integration
Storage and QRX-Net service effects are staged in the same QRXDB/WAL batch as wallet balances, nonces and applied-TX markers.
Locked supply pools include:
- consensus:storage:provider_bonds
- consensus:storage:provider_escrow
- consensus:storage:resilience
- consensus:qrxnet:domain_bonds

## Failure-domain attestation
The 3-of-N peer-attestation integration test now compiles and passes. Active providers can establish failure-domain metadata without a central CURA/geolocation authority.

## Authorized P2P shard serving
The existing QRX TCP P2P node now supports STORAGE_GET range requests.
A range is served only when:
- storage is explicitly enabled on the node;
- the local provider ID matches the active assignment;
- contract/shard/object IDs match QRXDB consensus state;
- requested range is within QRX_STORAGE_P2P_MAX_RANGE.

New node.conf keys:
- storage_enabled=0 (safe default)
- storage_provider_id=<wallet address by default>
- storage_path=<node_dir>/qrx-drive
- storage_max_usage_bytes=0
- storage_min_free_space_bytes=10737418240

## No artificial file-size limit
QRX Drive does not impose a product-level maximum file size such as 2 GB, 100 GB or 1 TB.
Large logical objects use qrx-drive-object-v1:
- uint64_t logical byte lengths;
- bounded independently encrypted chunks;
- default chunk size: 8 MiB;
- configurable chunk size: 64 KiB .. 64 MiB;
- one hybrid X25519MLKEM768-wrapped 256-bit file key per logical object;
- independent AES-256-GCM chunk encryption with per-chunk AAD;
- SHA3-256 content addressing for every chunk/index/descriptor;
- hierarchical 64-way CAS index pages;
- bounded memory during upload and restore;
- streamed restoration without loading the whole logical object into RAM.

The protocol still has finite machine-representation bounds: logical sizes are uint64_t and host filesystems have their own limits. Those are implementation/protocol bounds, not an artificial QRX product cap.

## Verification
Full RelWithDebInfo core build with QRX_REQUIRE_PQC=ON:
- 30 / 30 tests PASS
- includes attestation, authorized P2P serving and hierarchical large-object roundtrip (>64 chunks forcing a multi-level index tree)
