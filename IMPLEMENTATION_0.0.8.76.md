# QRX 0.0.8.76 — Directory Publisher, Storage Readiness & Domain Activation

This snapshot completes the first end-to-end PUBLIC_SIGNED QRX-Net website publishing path.

## Security boundary

PUBLIC_SIGNED website data is intentionally public. It is not processed by the PRIVATE_PQ encryption pipeline. Integrity and publisher authenticity are provided by ML-DSA-65 site signatures plus content, CAS and Merkle commitments. Private QRX Drive data remains on the separate mandatory PRIVATE_PQ path.

The directory walker rejects path traversal, unsafe relative paths, duplicate case-normalized paths, POSIX symlinks, Windows reparse points and non-regular special files. Website data is streamed rather than loaded as one complete site object.

## Publish lifecycle

1. Wallet owner selects an owned `.qrx` domain and local website directory.
2. Files are streamed into PUBLIC_SIGNED CAS and hashed/Merkle-committed.
3. An immutable ML-DSA-65 signed site version is archived.
4. A canonical QRXWEB1 distribution object is prepared.
5. The object is Reed-Solomon encoded as STANDARD 10+4 with exact CAS and PoStor commitments.
6. A deterministic storage contract and 14 assignments are submitted.
7. Exact prepared shards are uploaded to the assigned providers.
8. Provider acceptance/PoStor lifecycle activates assignments.
9. Only after at least 10 exact assignments are ACTIVE may the wallet submit DOMAIN_UPDATE for `web_manifest_root`.
10. The publish job becomes ACTIVE only after that root is visible in the authoritative DomainRecord.

## Restart/idempotency

`qrx-net-publish-job-v2` persists preparation, contract, transfer, pending storage transaction and activation state. Identical CREATE/ASSIGN steps are not repeatedly submitted while awaiting confirmation.

## Versioning and rollback

Historical `.qrxsite` packages are immutable. Rollback submits a fresh DOMAIN_UPDATE; providers do not rewrite old content. A target is eligible only when it verifies using the current domain publishing-key commitment and its own exact storage contract still has at least 10 ACTIVE shards.

## Interfaces

Daemon/CLI/Tauri now expose prepare, inspect, advance, version-list and rollback flows. The GUI provides native directory selection and an explicit PUBLIC_SIGNED publisher panel.

## Validation

- Core CTest: 51/51 PASS
- GUI inline JavaScript syntax: PASS
- Tauri `cargo check`: not executed because cargo is unavailable in the current build environment

## Next

0.0.8.77 — Verified QRX:// Fetch/Cache + Integrated WWW/QRX Browser Routing.
