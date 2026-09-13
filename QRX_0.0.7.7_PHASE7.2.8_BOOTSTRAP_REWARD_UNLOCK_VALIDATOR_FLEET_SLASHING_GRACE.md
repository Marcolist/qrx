# QRX 0.0.7.7 Phase 7.2.8 — Bootstrap Reward Unlock, Validator Fleet UX & Slashing Grace

## Goal
Make bootstrap validator behavior safe and obvious before Mainnet, especially when an operator owns many bootstrap wallets.

## Consensus / staking rules

### Genesis principal
Each bootstrap validator starts with the configured Genesis self-stake (currently 1,000 QUB) and a 180-day height lock.

During the lock:
- the Genesis principal remains self-staked;
- it cannot be transferred because it is not a liquid balance;
- it cannot be unstaked below the locked principal floor;
- it is immediately usable as validator power.

### Rewards and later incoming QUB are NOT locked
Validator rewards are credited to the normal liquid balance. QUB sent to the same address after Genesis is also normal liquid balance. Neither is added to the Genesis principal lock.

If a bootstrap validator later self-stakes additional QUB, that extra stake may be unstaked during the 180-day period as long as the remaining self-stake does not fall below the still-locked Genesis principal.

Example:
- Genesis principal: 1,000 QUB locked
- rewards: +40 QUB liquid
- later deposit: +60 QUB liquid
- manually stake another 50 QUB
- self-stake = 1,050 QUB, liquid = 50 QUB
- up to 50 QUB of the extra self-stake can be unstaked; the original 1,000 QUB floor remains locked.

### Bootstrap slashing grace
A blanket no-slash exemption is unsafe. Phase 7.2.8 therefore distinguishes liveness failures from Byzantine faults.

For the same 180-day bootstrap window:
- offline / liveness slash amount: 0 QUB
- offline jail: disabled for bootstrap identities while grace is active
- double-sign / equivocation slashing: ACTIVE from block 1
- double-sign tombstoning: unchanged

After the bootstrap lock/grace height is reached, bootstrap validators use exactly the same slashing rules as normal validators.

This protects early operators from shaky connectivity/immature network conditions without making malicious double-signing free.

## New Core status command

```bash
qrx bootstrap-validator-status <chain-dir> <validator-address>
```

Important fields:
- `is_bootstrap`
- `bootstrap_principal`
- `bootstrap_lock_until_height`
- `bootstrap_lock_active`
- `liveness_slash_grace_active`
- `double_sign_slashing_active`
- `self_stake`
- `locked_self_stake`
- `freely_unstakeable_self_stake`
- `spendable_balance`

`staking-status` now also exposes the bootstrap lock and grace fields for a selected address.

## GUI — Validator Fleet
The Validator page now has a fleet table designed for dozens or hundreds of wallet identities:
- wallet name + address
- bootstrap vs normal
- total self-stake
- locked Genesis principal
- liquid/spendable balance
- extra stake that can be unstaked now
- liveness grace status
- double-sign slashing status
- Validator Mode state
- select all / select bootstrap / clear
- batch enable / disable Validator Mode

Batch enable/disable never moves QUB and never auto-stakes a balance.

## Important runtime architecture guard
Phase 7.2.8 deliberately does not start 50 qrxd processes against the same chain database. That would create unsafe concurrent WAL/chain writers. The fleet screen can safely manage unlimited identities, but the current runtime still has one active block-producer wallet per daemon.

A proper high-density setup should be one synchronized node with a multi-key validator signer/proposer subsystem. That should be implemented as a separate consensus/runtime phase rather than faking it with dozens of daemons.

## User mental model

```text
Wallet / address
  ├─ Liquid balance       -> SEND / HOLD / STAKE
  ├─ Extra self-stake     -> can be unstaked normally
  └─ Genesis principal    -> 1,000 QUB bootstrap floor, locked 180 days

Bootstrap first 180 days
  ├─ offline problem      -> 0 QUB slash, no offline jail
  └─ double-sign          -> normal slash immediately

After 180 days
  └─ normal validator rules for everything
```
