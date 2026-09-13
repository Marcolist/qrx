# QRX 0.0.8.75 — PUBLIC_SIGNED Package Integrity + Immutable Versions

This step hardens the existing QRX-Net site primitives into a persistent, tamper-evident PUBLIC_SIGNED package layer.

## Security properties

- `.qrx` domain uniqueness is enforced by the normalized authoritative QRXDB registry. A second active/grace registration of the same normalized name is rejected regardless of spelling case.
- `QRXNS01` packages contain the immutable site manifest, file-entry commitments and ML-DSA-65 signature.
- Verification recomputes the bundle root from all file entries before trusting the manifest signature/domain root.
- Each locally fetched CAS object must also match its committed SHA3 content hash and Merkle root.
- Historical publisher versions are immutable files keyed by domain + sequence. Existing versions are never overwritten.
- Rollback only prepares a historical manifest root; the current domain owner must still authorize a new `DOMAIN_UPDATE` transaction.

## Validation

`net_phase116_site_package` covers package persistence, entry substitution, content tamper, immutable archive/rollback, and case-normalized duplicate-domain rejection.

Full QRX Core test suite: **49/49 PASS**.
