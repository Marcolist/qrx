# QRX 0.0.7.7 Phase 7.2.9 — Multi-Validator Signer / Validator Fleet Runtime

## Goal
Run many validator wallet identities on one synchronized QRX node without starting one `qrxd` process per wallet or sharing QRXDB/WAL between competing daemons.

## Runtime model

```
1 x qrxd / P2P / QRXDB / mempool
        |
        +-- validator wallet A signer
        +-- validator wallet B signer
        +-- validator wallet C signer
        +-- ...
```

The GUI passes every wallet marked `Validator Mode` to one daemon using repeated `--validator-wallet NAME` arguments. The daemon resolves the normal shared wallet directories and never copies private keys into a separate fleet store.

Proposal signing rotates through usable fleet wallets. A wallet that cannot sign with the active validator-session passphrase, is under-staked, jailed, tombstoned, or otherwise unusable is skipped rather than stalling the whole fleet. All usable fleet validator identities vote on the same proposal using identity-specific local double-sign locks.

The old single-validator behavior remains the fallback when no fleet wallets are configured.

## Security invariants
- One chain database and one P2P node per network instance.
- No automatic staking.
- Enabling a fleet wallet only makes it eligible as a signer on the next node start/restart.
- Each validator identity keeps a separate double-sign lock even though it shares the node runtime.
- Existing bootstrap 180-day principal lock and Phase 7.2.8 liveness-grace rules remain unchanged.
- Double-sign slashing remains active from block 1.
- Private keys remain in their existing wallet directories.
- Fleet wallets must be unlockable by the active validator-session passphrase for unattended signing. Unusable identities are skipped.

## Delegation status and UX
Delegation already existed in Core before Phase 7.2.9:
- `delegate`
- `undelegate`
- `claim-undelegated`
- validator-set / delegated power accounting
- validator commission and delegator reward split paths

The previous GUI only exposed `Delegate`, so the user flow was incomplete. Phase 7.2.9 adds:
- plain-language Delegator Mode explanation;
- explicit statement that the delegator computer may be offline;
- explicit exact-amount confirmation (never “delegate all”);
- validator-address validation guidance;
- two-step `undelegate -> unbond -> claim` UI;
- CLI bridge support for `undelegate` and `claim-undelegated`;
- explanation that delegation does not hand the validator a private key and does not enable Validator Mode.

There is no separate “delegated node” process. A delegator delegates stake to a validator node; the delegator itself does not need to operate infrastructure.

## Important pre-Mainnet status
The existing staking/delegation engine predates the later QRXDB atomic-consensus hardening and still contains legacy file-backed staking state / direct administrative command paths. Phase 7.2.9 makes the runtime and GUI complete and usable for alpha/regtest, but this does **not** by itself convert delegation state transitions into signed, network-broadcast, QRXDB-atomic consensus transactions. That remains a separate pre-Mainnet consensus-hardening item and must not be silently treated as audited Mainnet delegation.

## New/changed interfaces
Core:
- `qrx propose-block-as <node-dir> <wallet-dir> [max_txs]`
- `qrx vote-block-as <node-dir> <wallet-dir> <block-file>`

Daemon:
- repeated `--validator-wallet NAME`
- `getblockproducerinfo` reports `validator_fleet_enabled` and `validator_fleet_count`

GUI/Tauri:
- enabled Validator Fleet wallets are injected into the single daemon at start;
- Delegation UI now includes delegate, undelegate and claim flow.

## Compatibility
All Phase 7.2.8 bootstrap reward unlock, liveness grace, slashing behavior, Generals 7.2.7/7.2.6 fixes, installer packaging and prior QRX Core functionality are retained.
