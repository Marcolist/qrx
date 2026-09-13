# QRX 0.0.8.74 — QRX-Net Domain Management + PQ Ownership Wiring

This snapshot closes the wallet/daemon management gap around the existing QRX-Net consensus registry.

## Security invariants

- `.qrx` current state and history remain authoritative QRXDB consensus state.
- Domain mutation transactions continue to use the normal QRX hybrid transaction signing path.
- Website publishing identities are now ML-DSA-65 only at the key-commitment API boundary.
- Sequence numbers are mandatory for update/transfer/renew transitions and stale sequence replay fails.
- A domain transfer clears QUB routing, web-manifest root and publishing-key commitment before the new owner can republish.
- `.qrx` resolution continues to return `dns_allowed=false`.

## Management surfaces

Daemon/CLI now expose `getdomainpreflight`, `registerdomain`, `renewdomain`, `updatedomain`, `transferdomain`, `listdomains`, and `getdomainhistory` in addition to `getdomain` and `resolvebrowserinput`.

The Tauri wallet exposes equivalent commands and a QRX-Net domain manager view for availability/pricing, registration, renewal, ML-DSA publishing-key binding, history, and transfers.

## Tests

`net_phase115_domain_management` adds ML-DSA-only publishing-key checks, sequence anti-replay, historical transitions, owner-list scanning, and transfer-record clearing.

Full Core CTest result: 48/48 PASS.
