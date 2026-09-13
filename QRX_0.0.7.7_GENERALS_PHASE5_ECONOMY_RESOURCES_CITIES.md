# QRX Generals 0.0.7.7 — Phase 5: Economy, Resources, Cities & Strategic Objectives

Phase 5 adds deterministic economic geography to the serverless Generals consensus state.

## Deterministic strategic map features
Land hexes derive a feature from SHA3-512(`QRX-GENERALS-STRATEGIC-v1|season|x|y`). No central map database or operator placement is required.

- CITY: +50 supply, +25 materials, +5 score per collected turn
- OIL_FIELD: +75 fuel, +3 score
- MINE: +90 materials, +3 score
- SUPPLY_HUB: +100 supply, +25 ammo, +4 score
- COMMAND_CENTER: +15 score

Most tiles remain NONE. WATER never hosts these land objectives.

## Consensus transactions
- `GAME_STRATEGIC_CLAIM`: an active unit occupying a strategic feature captures it for its wallet. Ownership and owner counters are staged atomically. Recapture removes the previous owner's counter and assigns the new owner.
- `GAME_ECONOMY_COLLECT`: once per turn, converts the player's currently owned feature counters into Supply/Fuel/Ammo/Materials and economy score. Replay in the same turn is rejected.

Both operations use the existing signed Velocity transaction path, nonce/replay protection, QUB action cost, QRXDB batch/WAL and applied transaction marker.

## Read surface
- `qrx generals-strategic-info <chain> <season> <x> <y>`
- `qrx generals-economy-info <chain> <wallet> <season>`

## Design boundaries
This phase intentionally keeps economic resources as in-game consensus resources, not transferable QUB/native assets. QUB remains the network/anti-spam and treasury currency. Automatic city growth, markets/trading, clan pooled economies, season finalization and reward claims are later phases.
