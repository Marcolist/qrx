# QRX 0.0.7.7 Phase 7.2.13 — Home Validator Catch-up & Liveness Safety

Phase 7.2.13 makes Validator Mode safer for normal home connections without weakening the safety penalty for double-signing.

## Catch-up protection

`qrxd` now fails closed for validator signing on public networks. After an outage/reconnect it will not propose or vote while:

- there are fewer than 1 connected peers,
- the local chain is more than 2 blocks behind the best peer height, or
- the node has not yet remained within the safe window for 30 seconds.

Runtime states exposed by `getnodestatus` include `validator_signing_paused`, `validator_pause_reason`, `validator_catchup_blocks_behind`, and `validator_auto_resume=true`.

Normal recovery is therefore:

`OFFLINE -> CONNECTING -> CATCHING_UP -> SYNC_STABILIZING -> READY -> VALIDATING`

No user restart is required. Regtest deliberately permits zero peers.

## Home-friendly liveness profile

At the nominal 10-second block time:

- first 72 hours of one continuous outage: no liveness slash,
- first 0.05% liveness penalty only after a further full 24-hour interval (at about day 4),
- then 0.05% per additional 24 hours,
- hard cap: 1.00% per one continuous outage,
- temporary jail: only after 7 days offline, for 1 hour,
- once the validator is observed active again, the continuous-outage cap resets.

For a validator with only 1,000 QUB slashable power, one continuous non-bootstrap outage is therefore capped at about 10 QUB of liveness slash. If delegations contribute to validator power, liveness slashing is applied to slashable validator power according to the protocol's existing proportional slashing rules.

The separate Bootstrap rule remains stronger: during the 180-day bootstrap liveness grace, ordinary offline/liveness slashing remains 0 QUB and no offline jail is applied.

## Double-signing intentionally unchanged

Double-signing is a safety violation, not a home-internet availability problem. The 50% double-sign slash and tombstone behavior remain unchanged. Do not run the same validator private keys on two nodes at the same time.

## AURA / GUI

AURA Local Help and Validator Mode guidance now explain the catch-up state machine and the numerical home liveness profile.
