# QRX 0.0.7.6 — Native Assets & Regulated Tokenization — Mainnet Consensus Completion

## Goal

0.0.7.6 keeps every existing QRX/QUB key, qrx1 address, recovery seed and ordinary QUB transaction format intact. Native assets are an additive Velocity transaction family with an explicit height-aware `asset_activation_height` gate integrated with the existing protocol/fork parameter mechanism.

## Ravencoin-parity asset primitives

The consensus path now covers the Ravencoin asset model primitives used by the public Ravencoin asset layer:

- MAIN assets with globally unique names
- SUB assets (`PARENT/CHILD`)
- UNIQUE assets (`PARENT#SERIAL`), fixed quantity 1, units 0, never reissuable
- OWNER authority assets (`ASSET!`) created automatically where applicable
- REISSUE / remint with an issuance-time `reissuable` flag
- irreversible `reissuable=0`: once disabled it cannot be restored
- units/divisibility 0..8; units may increase on reissue but never decrease
- associated metadata
- message CHANNEL assets (`PARENT~CHANNEL`) and signed broadcasts
- QUALIFIER assets (`#KYC`)
- SUBQUALIFIER assets (`#KYC/#EU`)
- address qualifier TAG / UNTAG
- RESTRICTED assets (`$SEC`) with verifier expressions
- verifier grammar: qualifier atoms, `&`, `|`, `!`, parentheses, plus `true`
- per-address restricted freeze/unfreeze
- global restricted asset freeze/unfreeze
- ownership-authorized reissue and administration

QRX extensions for regulated tokenization additionally include capability-gated issuer REVOKE and FORCED_TRANSFER. These are opt-in asset capabilities and do not apply to QUB.

## Remint / reissue policy

At issuance the creator chooses:

`reissuable=1` — additional supply and permitted metadata/unit changes may later be made by the owner authority.

`reissuable=0` — supply is permanently fixed. Consensus rejects every later ASSET_REISSUE.

A reissuable asset can later be changed to `reissuable=0`. This is one-way and cannot be reverted.

`max_supply=0` means no additional issuer-selected cap beyond integer/consensus limits. A positive max_supply is a hard ceiling enforced on every reissue.

## KYC providers

A provider can control a qualifier asset such as `#KYC_CURA`. Holding the qualifier authority permits it to TAG or UNTAG QRX addresses. A regulated issuer can then create a restricted asset with e.g.:

`verifier=KYC_CURA`

or

`verifier=KYC_CURA&EU_RETAIL`

Transfers are consensus-rejected unless the restricted policy is satisfied. This reuses QRX provider/attester governance without putting names, ID documents or other KYC PII into the asset itself.

## Mainnet mutation path

Mainnet asset state is not changed by the old local `asset-*-v1` helpers. Those helpers are explicitly dev/regtest-only. Mainnet mutations use signed hybrid QRX transactions and the existing network/genesis/protocol binding, lane nonce and replay protection.

Supported consensus transaction types:

- ASSET_ISSUE
- ASSET_REISSUE
- ASSET_TRANSFER
- ASSET_TAG / ASSET_UNTAG
- ASSET_FREEZE_ADDRESS / ASSET_UNFREEZE_ADDRESS
- ASSET_GLOBAL_FREEZE / ASSET_GLOBAL_UNFREEZE
- ASSET_BROADCAST
- ASSET_REVOKE
- ASSET_FORCED_TRANSFER

The transaction is created with `create-velocity-raw-tx`, signed with `signrawtransactionwithwallet`, verified with `verify`, and submitted through the normal node transaction path (`sendtx`; `applytx` is the deterministic state application primitive).


## Asset activation and operation burns

`asset_activation_height` provides deterministic activation. Transactions before that height are rejected. Regtest/pre-Genesis defaults to `0`; Mainnet may set a later genesis/fork height.

Ravencoin-style anti-spam economics are represented by QRX-native configurable QUB burns for main/sub/unique/channel/qualifier/subqualifier/restricted issuance, reissue and tag operations. Burned atoms are accounted under the authoritative consensus key `consensus:asset76:burned_qub_atoms`. Values are QRX parameters, not copied Ravencoin economics.

## RPC read surface

Alongside the existing `createvelocitytransaction`, `signrawtransactionwithwallet` and `sendrawtransaction` path, the daemon/CLI exposes `getassetbalance`, `listassets`, `getassetinfo`, `getassettag`, `getassetrestriction` and `getassetburnedfees`.

## Atomicity

Asset metadata, balances, qualifier/restriction state, QUB transaction fee, lane nonce, applied-tx marker and transaction index are staged into one QRXDB batch and committed through the existing WAL. A failed asset policy check aborts the batch. The resulting QRXDB generation and Merkle state root cover the authoritative state.

## Security properties

- Existing qrx1 addresses and keys are unchanged.
- Asset transactions are hybrid Ed25519 + ML-DSA signed exactly like the existing Velocity transaction path.
- `from` must match the signing wallet public key.
- Transactions are bound to network ID, genesis hash and protocol version.
- Lane nonces provide replay protection.
- Reissuability cannot be resurrected after being disabled.
- Unique/qualifier/channel issuance rules are consensus checked.
- Parent ownership is required for sub/unique/channel assets.
- Restricted transfer policy, address freezes and global freezes are consensus checked.
- Self asset transfers are balance-neutral and cannot mint supply.
- QUB itself has no issuer freeze, revoke, forced-transfer or remint capability.

## Release gate

The native host audit includes a clean Core build, existing regression/privacy/governance gates, the legacy regulated-asset compatibility test and a signed consensus asset test covering issuance, owner tokens, reissue lock, increased units, sub assets, unique assets, qualifiers/subqualifiers, restricted verifier enforcement, tags, address/global freeze, channels/broadcasts, revoke, forced transfer, self-transfer conservation and qrx1 address compatibility.

"Mainnet-ready" here means there is no intentional local/manual Mainnet asset mutation path and asset state uses the signed atomic consensus path. It is not a substitute for an independent pre-Genesis security/consensus audit. The experimental Shielded-QUB cryptography retains its separate external-audit requirement.

## Anti-spam burn schedule (0.0.7.6 final)

The initial QRX block subsidy is `25,000,000 atoms = 0.25 QUB` at a 10-second target block time. The original asset burn defaults were too close to the emission rate and made namespace/state spam unnecessarily cheap. The Mainnet defaults are therefore raised substantially:

| Operation | Burn | Initial block-subsidy equivalents |
|---|---:|---:|
| MAIN issue | 100 QUB | 400 blocks |
| SUB issue | 25 QUB | 100 blocks |
| UNIQUE issue | 2.5 QUB | 10 blocks |
| CHANNEL issue | 25 QUB | 100 blocks |
| QUALIFIER issue | 250 QUB | 1,000 blocks |
| SUBQUALIFIER issue | 25 QUB | 100 blocks |
| RESTRICTED issue | 500 QUB | 2,000 blocks |
| REISSUE | 10 QUB | 40 blocks |
| TAG / UNTAG | 1 QUB | 4 blocks |

These QUB amounts are burned, not paid to the issuer or Development Fund. Normal asset transfers only pay the normal network transaction fee. The schedule is deliberately expensive for scarce namespaces and regulated-policy state, while keeping unique assets and routine compliance maintenance usable. Height-aware chain parameters retain the ability to change economics through an explicit protocol/governance upgrade rather than a wallet-side setting.
