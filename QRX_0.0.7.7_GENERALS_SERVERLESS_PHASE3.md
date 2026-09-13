# QRX 0.0.7.7 — Generals Serverless Phase 3: Deterministic World & Orders

Phase 3 moves the first authoritative game-world mechanics into QRX consensus. It extends Phase 1/2 without replacing wallet identities, General unique assets, clan membership, season entry, Energy or the Generals Treasury.

## Consensus transaction types added

- `GAME_ORDER_COMMIT`
- `GAME_ORDER_REVEAL`

Movement is intentionally executed through commit/reveal rather than exposing the complete order during the first half of a turn.

## Deterministic world

Each season has a deterministic 128 x 128 world by default. Terrain is derived from SHA3-512 using the domain-separated input:

`QRX-GENERALS-TERRAIN-v1|season_id|x|y`

Current terrain classes are `PLAINS`, `FOREST`, `HILLS`, and `WATER`. No terrain database or central map server is authoritative.

Consensus parameters exposed through `getparams`:

- `generals_world_width=128`
- `generals_world_height=128`
- `generals_order_commit_cost_atoms=10000`

## Starter army

`GAME_SEASON_JOIN` now atomically creates a deterministic starter army for the player's General:

- 1 HQ, immobile, 1000 HP
- 3 INFANTRY, 100 HP, movement range 1, movement Energy cost 1

Unit IDs are derived from season, wallet identity and ordinal using SHA3-512. Spawn tiles are deterministic, non-water and unoccupied. Occupancy is authoritative QRXDB state.

## Turn order protocol

A turn is split into two deterministic halves:

1. COMMIT phase — submit `GAME_ORDER_COMMIT` with the SHA3-512 commitment only.
2. REVEAL phase — submit `GAME_ORDER_REVEAL` with the order fields and secret.

For a movement order the commitment preimage is exactly:

`QRX-GENERALS-ORDER-v1|season_id|turn_id|MOVE|unit_id|to_x|to_y|secret`

The reveal is rejected if the commitment does not match, the player is not the unit owner, the destination is invalid/water/occupied, movement exceeds range, Energy is insufficient, the order is in the wrong season/turn/phase, or the unit already moved in the turn.

A successful reveal atomically updates:

- source tile occupancy
- destination tile occupancy
- unit x/y
- unit last-move turn and tx
- player Energy
- order status and reveal metadata
- Generals Treasury contribution
- normal nonce/applied-tx/tx-index state

The whole transition is one QRXDB WAL commit. Failed reveals do not partially move a unit or debit game state.

## Read-only inspection commands

- `qrx generals-world-info <chain-dir> [height]`
- `qrx generals-player-army-info <chain-dir> <wallet> [season-id]`
- `qrx generals-unit-info <chain-dir> <unit-id>`
- `qrx generals-order-info <chain-dir> <order-id>`
- `qrx generals-tile-info <chain-dir> <season-id> <x> <y>`

Existing Phase 1/2 commands remain available.

## Scope boundary

Phase 3 intentionally does **not** implement combat, production, territory capture, fog-of-war, supply, scoring, season finalization or reward claims yet. Moving into an occupied tile is rejected rather than treated as combat. Those mechanics should be layered on top of the deterministic world/order foundation in later phases.
