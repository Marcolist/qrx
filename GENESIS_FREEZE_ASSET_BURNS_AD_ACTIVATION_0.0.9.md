# QRX 0.0.9 Genesis Freeze — Asset Burns, Advertising Gate & Genesis Memo

## Canonical Genesis memo

The canonical memo is consensus data in `genesis.cfg`:

> At the threshold of the AI age, amid global change, QRX was launched to keep value, verification, and digital sovereignty in the hands of people -- and to help ensure that the opportunities of artificial intelligence are open to all, not reserved for a few.

It is included in the Genesis hash and has not been changed by this hardening phase.

## Subsidy-relative native-asset burns

`asset_burn_policy=block_reward_units_v1` is now part of newly generated Genesis configuration. Burn units:

- MAIN: 400 block-reward units
- SUB: 100
- UNIQUE: 10
- CHANNEL: 100
- QUALIFIER: 1000
- SUBQUALIFIER: 100
- RESTRICTED: 2000
- REISSUE: 40
- TAG / UNTAG: 4

At the 0.25-QUB Genesis subsidy these equal the previous 100 / 25 / 2.5 / 25 / 250 / 25 / 500 / 10 / 1 QUB quotes. After halvings they scale with the current subsidy. Legacy fixed-atom fields remain in Genesis for backward-readable baseline/reference, but consensus uses reward-unit pricing when `block_reward_units_v1` is selected. A minimal one-atom-per-unit floor remains after subsidy exhaustion so asset namespace operations never become completely free.

## Independent Advertising activation

`ADVERTISING_V1` is a new threshold-signed feature flag. Mainnet advertising consensus is accepted only when both:

1. `QRX_NET_V1` is active at the transaction height, and
2. `ADVERTISING_V1` is active at the transaction height.

The check exists inside the QRX-Net consensus preparation layer as well as top-level transaction admission, preventing a direct API caller from bypassing the feature gate.

Operational targets (not automatic consensus switches):

- DRIVE_V1 — 2026-11-30 17:00 UTC
- QRX_NET_V1 — 2026-12-07 17:00 UTC
- ADVERTISING_V1 — 2026-12-15 17:00 UTC
- COMPUTE_POUC_V1 — 2027-01-31 17:00 UTC

Final activation uses governance-selected block heights after readiness criteria pass.

## Regression

Phase 179 verifies the canonical Genesis memo, reward-unit asset-burn policy, first-halving scaling, independent advertising activation, and a direct consensus-API bypass attempt.
