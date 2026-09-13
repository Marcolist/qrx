# QRX 0.0.7.6 – Native Assets & Regulated Tokenization Layer

## Status

0.0.7.6 implements native assets as an **additive signed consensus transaction family**. Existing QRX/QUB private keys, `qrx1` addresses, recovery material and ordinary QUB transaction semantics are not migrated or replaced.

The old `asset-*-v1` direct mutation helpers remain only for development/regtest compatibility and are blocked by the existing manual-mint/dev-network gate on Mainnet. Mainnet asset mutations use signed Velocity transactions, network/genesis/protocol binding, lane nonces, QRXDB WAL batching, applied-transaction replay protection and the chain state root.

## Ravencoin-style native asset model

Implemented consensus asset kinds:

- `MAIN` – globally unique root asset
- `SUB` – `PARENT/CHILD`
- `UNIQUE` – `PARENT#SERIAL`, quantity 1, units 0, permanently non-reissuable
- `CHANNEL` – `PARENT~CHANNEL`, message/broadcast authority
- `QUALIFIER` – `#KYC`
- `SUBQUALIFIER` – `#KYC/#EU`
- `RESTRICTED` – `$SECURITY`
- `OWNER` – internally generated `ASSET!` ownership authority token

Parent ownership is required for sub-assets, unique assets and channels. Ownership tokens authorize reissue and issuer administration for the corresponding asset.

## Reissuable / remint policy

The issuer chooses the supply policy **at issuance**:

- `reissuable=1` – later `ASSET_REISSUE` transactions may mint additional supply, subject to the asset's `max_supply` and owner authority.
- `reissuable=0` – later remint/reissue is permanently forbidden by consensus.

A reissuable asset may later be changed from `1` to `0`. This transition is **one-way**. Consensus rejects attempts to restore reissuability afterward.

`units` are 0..8. Reissue may increase units but never decrease them. `max_supply=0` means no additional issuer-selected cap beyond consensus/integer limits; a positive `max_supply` is a hard ceiling.

## Standard and regulated tokenization

Standard assets can be freely transferred according to their asset rules. Regulated tokenization is implemented with Ravencoin-style qualifier/restricted primitives plus QRX-specific opt-in compliance controls.

A KYC provider can control a qualifier such as `#KYC_CURA` and tag/untag eligible QRX addresses. A restricted token can require a verifier expression such as:

```text
KYC_CURA
KYC_CURA&EU_RETAIL
(KYC_CURA&EU)|ACCREDITED
KYC_CURA&!US_PERSON
```

Supported verifier operators are `&`, `|`, `!`, parentheses and `true`. Restricted transfers are rejected by consensus when sender/receiver policy, per-address freeze or global-freeze requirements are not satisfied.

The asset state stores eligibility tags, not KYC documents. Names, ID scans and other PII remain outside the asset state.

## Regulated QRX extensions

At issuance a restricted asset may opt into immutable capabilities:

- `ISSUER_REVOKE`
- `ISSUER_FORCED_TRANSFER`

An asset that did not opt into one of these powers cannot use it later. These controls apply only to that asset. **Native QUB has no issuer freeze, revoke, forced-transfer or remint capability.**

## Consensus transaction types

```text
ASSET_ISSUE
ASSET_REISSUE
ASSET_TRANSFER
ASSET_TAG
ASSET_UNTAG
ASSET_FREEZE_ADDRESS
ASSET_UNFREEZE_ADDRESS
ASSET_GLOBAL_FREEZE
ASSET_GLOBAL_UNFREEZE
ASSET_BROADCAST
ASSET_REVOKE
ASSET_FORCED_TRANSFER
```

Transactions use the existing hybrid QRX signing path (Ed25519 + ML-DSA), network ID, genesis hash, protocol version and lane nonce. Asset state, QUB transaction fee, asset-operation burn, nonce, applied marker and transaction index are staged into one QRXDB batch.

## Activation and anti-spam economics

The feature is controlled by the chain parameter:

```text
asset_activation_height
```

Asset transactions before the configured activation height are rejected. Pre-Genesis/regtest defaults use height `0`; Mainnet genesis/fork configuration may set the intended activation height.

Asset operations also carry deterministic QUB burn fees. Current default chain parameters are:

```text
asset_issue_main_burn_atoms=10000000000
asset_issue_sub_burn_atoms=2500000000
asset_issue_unique_burn_atoms=250000000
asset_issue_channel_burn_atoms=2500000000
asset_issue_qualifier_burn_atoms=25000000000
asset_issue_subqualifier_burn_atoms=2500000000
asset_issue_restricted_burn_atoms=50000000000
asset_reissue_burn_atoms=1000000000
asset_tag_burn_atoms=100000000
```

These are QRX economics and can be governed through the existing height-aware chain-parameter mechanism; they are not intended as a copy of Ravencoin's numerical fees.

## RPC / wallet integration surface

Existing signed-transaction RPCs are used to create/sign/broadcast asset transactions. Read-only RPCs additionally expose:

```text
getassetbalance
listassets
getassetinfo
getassettag
getassetrestriction
getassetburnedfees
```

This gives GUI and third-party wallet clients a Mainnet-safe interface without direct state-file mutation.

## Compatibility guarantee for 0.0.7.6

The implementation is additive and regression-tested for:

- existing QRX private keys unchanged
- existing `qrx1` addresses unchanged
- recovery material unchanged
- existing QUB balances/transaction semantics unchanged
- no issuer authority over QUB

One QRX address can therefore hold QUB and multiple native assets without an address-format migration.

## Validation

`qrx-core/tests/native_assets_mainnet_consensus_0076.sh` covers signed issuance, ownership tokens, remint, irreversible reissuability lock, unit increase, sub/unique assets, qualifiers/subqualifiers, restricted verifier policy, tags, per-address/global freeze, channels/broadcasts, revoke, forced transfer, self-transfer conservation, burn accounting, introspection and existing-address compatibility.

`qrx-core/tests/native_assets_regulated_0076.sh` preserves the earlier regulated-asset regression coverage.

## Mainnet readiness boundary

The asset mutation path is now designed for Mainnet consensus: there is no intentional direct local Mainnet mutation path, signed transactions are replay-bound, and authoritative state is committed atomically through QRXDB/WAL/state-root infrastructure.

This is **not a claim of third-party security certification**. A pre-Genesis independent consensus/security audit remains recommended before valuable public issuance. The experimental Shielded-QUB cryptography separately retains its external cryptography-audit gate.
