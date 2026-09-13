# QRX 0.0.7.7 – QRX Generals Serverless Migration Phase 2

Phase 2 extends the Phase-1 authoritative QRXDB/WAL game state with deterministic seasons, block-derived turns, regenerating energy, season entry/prize reservation, and complete basic clan membership flows.

## Consensus transactions

Existing:
- `GAME_JOIN`
- `GAME_CLAN_CREATE`
- `GAME_TREASURY_FUND`

Added in Phase 2:
- `GAME_SEASON_JOIN`
- `GAME_ENERGY_SPEND`
- `GAME_CLAN_INVITE`
- `GAME_CLAN_INVITE_REVOKE`
- `GAME_CLAN_JOIN`
- `GAME_CLAN_LEAVE`

All are signed Velocity/QRX transactions and use the existing network/genesis/protocol binding, lane nonce replay protection, transaction index, applied-tx marker, and atomic QRXDB WAL commit.

## Seasons and turns

Seasons are not created by a central server. They are derived from consensus parameters and block height.

Defaults:
- `generals_season_start_height=0`
- `generals_season_length_blocks=259200` (about 30 days at 10 second blocks)
- `generals_turn_length_blocks=60` (about 10 minutes at 10 second blocks)
- `generals_season_join_window_blocks=0` (0 = joining allowed throughout the season; can be restricted by chain parameter)

A transaction that targets a season must include the current `season_id`. Energy/action transactions additionally bind to `turn_id`, preventing an old signed action from silently becoming valid in a later turn.

## Season entry and reserved prize pool

Default:
- `generals_season_join_cost_atoms=100000000` = 1 QUB

`GAME_SEASON_JOIN` atomically:
1. debits the player's QUB,
2. credits the on-chain Generals treasury,
3. increases `generals:treasury:reserved_atoms`,
4. increases the specific season's `prize_pool_atoms`,
5. registers the player and General for the season.

The full default season entry amount is therefore treasury-custodied but reserved for the season prize pool. It is not available for unrelated treasury spending. Phase 2 deliberately implements no reward payout path yet.

## Energy

Defaults:
- `generals_energy_max=100`
- `generals_energy_regen_blocks=6`
- `generals_energy_regen_amount=1`
- `generals_action_cost_atoms=10000` = 0.0001 QUB for `GAME_ENERGY_SPEND`

Energy regeneration is deterministic and lazy: nodes derive regenerated energy from the last committed energy height and current chain height. No cron job, server timer, or database worker exists.

`GAME_ENERGY_SPEND` is the Phase-2 consensus primitive used to validate the energy accounting model. Future movement/build/combat transactions should call the same energy accounting path atomically rather than trusting GUI-side values.

## Clan communities and invitation IDs

Phase 2 adds wallet-bound invitation IDs:

`CINV-<24 hex chars from signed transaction body hash>`

`GAME_CLAN_INVITE` is currently leader-authorized and stores:
- invite ID,
- clan ID,
- inviter,
- invited player wallet,
- created height/tx,
- expiry height,
- status.

Default invite cost:
- `generals_clan_invite_cost_atoms=100000` = 0.001 QUB

Default maximum lifetime:
- `generals_clan_invite_max_blocks=604800`

The invited wallet accepts with `GAME_CLAN_JOIN`. An invite becomes `accepted` and cannot be reused. The leader can revoke a pending invitation with `GAME_CLAN_INVITE_REVOKE`.

Members can leave via `GAME_CLAN_LEAVE`. Founders/leaders cannot leave in Phase 2 until a future leadership-transfer transaction exists, preventing leaderless clans.

Low-cost state-changing membership operations use:
- `generals_clan_membership_cost_atoms=10000` = 0.0001 QUB

These amounts are credited to the Generals treasury in addition to any normal network transaction fee.

## Read-only inspection commands

- `generals-player-info <chain-dir> <wallet-address>`
- `generals-clan-info <chain-dir> <clan-id>`
- `generals-clan-invite-info <chain-dir> <invite-id>`
- `generals-season-info <chain-dir> [height]`
- `generals-energy-info <chain-dir> <wallet-address> [height]`
- `generals-treasury-info <chain-dir>`

Treasury info now distinguishes:
- `balance_atoms` – all QUB held by the protocol game treasury,
- `reserved_atoms` – QUB already committed to season prize pools,
- `available_atoms` – unreserved treasury balance.

## Deliberate Phase-2 boundaries

Not yet implemented:
- map/territory state,
- unit creation and movement,
- combat resolution,
- commit/reveal orders,
- season ranking/finalization,
- reward claims/payouts,
- booster purchases,
- General transfer transaction,
- clan leadership transfer/officer roles.

Those should build on this state rather than reintroducing a central game server.

## Compatibility

No changes are made to existing QRX private keys, qrx1 addresses, seed/recovery format, QUB transaction semantics, or the native `GENERAL#...` unique-asset ownership model introduced in Phase 1.
