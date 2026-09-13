# QRX 0.0.7.2 Privacy Layer – Phase 2: Stealth Hardening

## Scope

This phase hardens the existing QRX X25519 stealth-address prototype without claiming that the complete consensus spend path is ready for public funds.

## Security changes

- Removed the public-address fallback for private stealth scan/spend key derivation.
- Stealth private keys are now derived only from the decrypted primary Ed25519 private key using the domain `QUB-X25519-STEALTH-v3`.
- Locked/unavailable wallets fail closed; even an empty legacy passphrase must be explicitly provided through the wallet session.
- Scan and spend X25519 private material is deterministic from the recoverable primary wallet key, so restoring the same QRX recovery pair reproduces the same stealth identity.
- Sensitive temporary key/shared-secret buffers are cleansed after use.

## Metadata minimization

The public `stealth_transfers.db` format is upgraded to metadata v2:

`tx_id|sender|one_time_address|amount|ephemeral_pub|created_at|status|memo`

The following are no longer persisted:

- recipient `squb1...` stealth address
- raw X25519 shared secret / shared tag

Legacy records are still readable. On the next write they are migrated to v2 and the legacy recipient stealth address/shared secret fields are discarded.

Recipient history is no longer identified by comparing a stored stealth address. The wallet re-derives the X25519 shared secret locally with its private scan key and checks the expected one-time address.

## Recovery

A restored wallet with the same primary Ed25519 private key deterministically reproduces the same X25519 scan/spend keys and therefore the same `squb1...` stealth address. No extra standalone stealth seed is required.

## Audit boundary

`privacy-feature-status` now reports:

- `stealth_addresses=phase2-hardened-x25519-experimental`
- `stealth_key_derivation=private-wallet-material-only`
- `stealth_public_metadata=recipient-stealth-address-and-shared-secret-not-persisted`
- `stealth_recovery=deterministic-from-recovered-primary-ed25519-key`
- `stealth_spend_path=consensus-audit-required-before-public-funds`

This phase intentionally does **not** claim that the current one-time-address spend/claim mechanism is production-ready. A consensus-level spend-path redesign/audit remains required before recommending stealth transfers for public Mainnet funds.

## Tests

New test: `qrx-core/tests/privacy_phase2_stealth_hardening.sh`

Validated:

1. locked wallet cannot derive private stealth keys;
2. distinct wallets create distinct X25519 stealth identities;
3. wallet recovery reproduces the exact same stealth address;
4. public transfer DB contains neither recipient stealth address nor raw shared secret;
5. old leaky records are rewritten into metadata v2;
6. recipient scanning still discovers its payment using its private scan key;
7. recipient history ownership is derived cryptographically;
8. feature status exposes the remaining audit boundary.

## Regression / release audit

The complete QRX 0.0.7.2 native-host audit passed:

- cross-platform build plans: PASS
- GUI/Core RPC compatibility: 29/29 PASS
- network/RPC hardening: PASS
- 0.0.6 regression surface: PASS
- Python tooling/wallet/build tests: PASS
- GUI JavaScript syntax: PASS
- native Core clean build: PASS
- VELOCITY CTests: 6/6 PASS
- Privacy Layer Phase 1 surface: PASS
- Privacy Layer Phase 2 Stealth Hardening: PASS
- 0.0.7.2 packaging/version gates: PASS

Native installers for macOS x64/arm64, Windows x64 and Linux ARM64 still require their native CI/build runners, as before.
