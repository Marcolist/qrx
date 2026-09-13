# QRX 0.0.7.4 — Privacy Layer Phase 4
## Hidden Balances + Verified Privacy

Phase 4 adds an optional verified-privacy layer on top of the Phase 3 shielded pool.

### Design goal

A user may use the **Verified Privacy / Hidden Balance** path only while holding a valid privacy credential from an accepted attester. Identity/KYC data itself never enters QRX chain state.

The intended production flow is:

1. An accepted attester (for example CURA) performs KYC off-chain.
2. The attester issues a signed QRX privacy credential to the wallet.
3. The credential is stored locally in the wallet under `privacy/verified_privacy.cred`.
4. QRX verifies issuer status, holder binding, signature, expiry and revocation before verified privacy actions.
5. Shielded notes continue to use the Phase 3 encrypted-note / commitment / range-proof / key-image design.
6. Public chain state contains only accepted attester public keys/status and opaque credential revocation hashes — not names, addresses, dates of birth, document numbers or KYC files.

### Accepted attester registry

Backend maintenance commands:

- `qrx privacy-attester-register <chain-dir> <issuer-id> <issuer-public.pem>`
- `qrx privacy-attester-disable <chain-dir> <issuer-id>`

Registry record:

`issuer_id | Ed25519 public key | ACTIVE/DISABLED | timestamp`

No customer identity is stored here.

**Mainnet governance gate:** these direct maintenance commands are an offline/bootstrap state-management primitive. Before Mainnet governance is finalized, accepted-attester additions/removals must be converted into authenticated consensus/governance transactions. They are intentionally not exposed as normal wallet RPC methods.

### Credential issuance

Offline attester tool:

`qrx privacy-credential-issue <chain-dir> <wallet-dir> <issuer-id> <issuer-private.pem> <valid-until-unix>`

The credential is Ed25519-signed and binds:

- accepted issuer id
- a salted wallet-holder commitment
- an opaque random serial hash
- issue time
- expiry time
- policy `hidden-balance-v1`

The local credential contains the random salt and serial required for holder verification. These values are not written to chain state.

### Credential verification

`qrx privacy-credential-status <chain-dir> <wallet-dir>`

Verification is fail-closed and checks:

- credential format
- accepted/active issuer
- expiry / not-before window
- salted wallet subject binding
- serial binding
- issuer Ed25519 signature
- opaque revocation hash

Tampering any signed or holder-bound field invalidates the credential.

RPC/CLI exposes read/use operations only:

- `privacy-credential-status`
- `hidden-balance`
- `verified-shield`
- `verified-shielded-send`
- `verified-unshield`

Attester registry mutation and credential issuance are not normal wallet RPC operations.

### Hidden balance

`hidden-balance` is a credential-gated view of the wallet's Phase 3 shielded balance.

An unverified, expired, revoked or tampered credential is rejected before the balance is decrypted/read.

The public chain still sees only shielded commitments/ciphertexts/proofs. It does not expose the user's shielded total.

### Verified shielded actions

- `verified-shield <amount> [zqub2-address]`
- `verified-shielded-send <zqub2-address> <amount>`
- `verified-unshield <qrx-address> <amount>`

Each command requires a currently valid privacy credential before delegating to the Phase 3 shielded transaction path.

The original experimental Phase 3 commands remain available for development/regression compatibility. Production GUI policy should prefer the `verified-*` path whenever Hidden Balance mode is enabled.

### Revocation

`qrx privacy-credential-revoke <chain-dir> <opaque-serial-hash>`

Only the SHA3-512 opaque serial hash is written to revocation state. The raw serial and identity are not written on-chain.

### No protocol-wide backdoor

Phase 4 does **not** create a CURA master viewing/decryption key and does not allow an attester to decrypt arbitrary shielded balances.

The attester proves only that it issued a currently acceptable credential. Selective disclosure remains a separate holder-controlled capability and should be implemented with purpose-specific disclosure/view proofs rather than a universal surveillance key.

### GUI

The Privacy Center now explains:

- Hidden balances require a verified privacy credential.
- KYC/identity data remains off-chain.
- Accepted issuer state contains public keys/status only.
- QRX does not create a universal viewing/backdoor key.

### Validation

Native host audit results:

- Core clean build: PASS
- GUI/Core compatibility regression: PASS
- network/RPC hardening: PASS
- 0.0.6 regression surface: PASS
- Privacy Phase 1: PASS
- Privacy Phase 2: PASS
- Privacy Phase 3: PASS
- Privacy Phase 4 credential gate: PASS
- hidden-balance blocked without credential: PASS
- accepted issuer signature validation: PASS
- holder binding: PASS
- credential expiry checks: PASS
- opaque revocation: PASS
- revoked credential rejection: PASS
- tampered credential rejection: PASS
- no wallet address / KYC payload in chain credential state: PASS
- VELOCITY CTests: 6/6 PASS
- package/CI version gate: 0.0.7.4 PASS

### Remaining production gates

Two independent external/security gates remain before treating this privacy stack as production Mainnet privacy for real funds:

1. The Phase 3 custom shielded cryptography still requires an independent cryptography/consensus audit.
2. Accepted-attester registry mutations must be represented by authenticated consensus/governance transactions rather than offline direct chain-state maintenance.

KYC operational/legal procedures, retention, lawful requests and attester policy are application/compliance concerns and must remain separate from QRX consensus identity data.
