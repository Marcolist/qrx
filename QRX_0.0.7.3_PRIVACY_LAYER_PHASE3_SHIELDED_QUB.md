# QRX 0.0.7.3 Privacy Layer – Phase 3: One-Time Claim/Spend + Shielded QUB

## Scope

This release candidate completes the wallet-local/chain-state One-Time Address claim/spend path introduced by Stealth Phase 2 and replaces the old cleartext shielded-note skeleton with a cryptographic Phase-3 design.

## Stealth One-Time Address Claim/Spend

- `squb2` address format: X25519 scan public key + secp256k1 spend public key.
- Sender derives a unique secp256k1 one-time public key from receiver spend public key + X25519 shared secret.
- Receiver reconstructs the matching one-time private key only from private wallet material.
- `stealth-spend` requires a secp256k1 ECDSA ownership proof over a domain-separated spend body.
- Spend body is bound to QRX `network_id` and `genesis_hash`, preventing cross-network signature replay.
- Per-one-time-address replay nonce and value movement are committed in the same `balances.bin` rewrite.
- Partial spends are supported; the output becomes `spent` only when its remaining balance reaches zero.
- Wrong-wallet spends and overspends fail closed.
- RPC/CLI parity includes `stealth-spend`.

## Shielded QUB Phase 3

Public shielded state no longer stores plaintext `owner`, `value`, `rho`, randomness, or memo witnesses.

### Addressing and note encryption

- `zqub2` address: X25519 view public key + secp256k1 spend public key.
- Each note uses an ephemeral X25519 key agreement.
- Note witness is encrypted with AES-256-GCM under a SHA3-256 domain-separated shared-secret KDF.
- Public note data contains commitments, one-time public key, ephemeral public key, ciphertext, AEAD tag and proof hash.

### Value commitments / proofs

- secp256k1 Pedersen commitments hide note values.
- 63-bit Fiat-Shamir OR range proofs prove note values are in range without publishing the value.
- Proof files are content-hashed and public-note verification fails closed if a proof is changed.

### Spend privacy and double-spend protection

- Shielded inputs use linkable ring-style ownership/membership proofs over mixed public one-time keys.
- The real input index is not written to the public spend journal.
- Key images/nullifiers prevent re-spending an already-consumed shielded input.
- A Merkle commitment tree tracks public shielded note leaves/root.

### Value conservation and change

- Shielded → shielded uses pseudo-output commitments and verifies a global commitment balance equation.
- If selected inputs exceed the payment, a shielded change note is mandatory.
- Shielded → transparent (`unshield`) explicitly binds the public amount into the balance equation and creates shielded change for the remainder.
- This fixes the earlier skeleton bug where spending 30 from a 100-QUB note could consume the entire note without a 70-QUB change output.

## Security / release boundary

The implementation is a functional cryptographic release candidate, not a substitute for an independent cryptography/consensus audit. `privacy-feature-status` intentionally reports:

`mainnet_release_gate=external-cryptography-audit-required-before-real-funds`

Therefore “mainnet-ready” in this tree means the One-Time Address spend path now has ownership proofs, chain replay binding, atomic value+nonce state, RPC/CLI parity and regression tests. Real-value public Mainnet activation of the new privacy cryptography remains gated on independent review.

## Automated validation

`qrx-core/tests/privacy_phase3_shielded_and_stealth_spend.sh` validates:

- non-owner cannot claim a one-time output;
- one-time ownership proof verifies;
- partial spends use monotonically increasing replay nonces;
- overspend/re-spend fails;
- one-time value conservation;
- encrypted shielded deposit;
- shielded payment with mandatory change;
- receiver and sender balances conserve value;
- plaintext witnesses are absent from the public shielded state;
- Merkle leaves, nullifier/key-image state and proof directory exist;
- unshield preserves remainder via change note;
- tampered range proof is rejected;
- release status exposes the external-audit gate.

The full `scripts/final-release-audit.sh` now includes Phase 1, Phase 2 and Phase 3 privacy gates and packages release artifacts as QRX `0.0.7.3`.
