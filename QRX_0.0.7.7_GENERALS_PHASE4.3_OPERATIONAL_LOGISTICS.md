# QRX Generals 0.0.7.7 Phase 4.3 — Operational Logistics, Fuel, Ammunition & Encirclement

Phase 4.3 makes the Phase 4.2 supply graph operational.

## Consensus changes
- Movement consumes per-unit fuel and is rejected when fuel is insufficient.
- Infrastructure attacks consume per-unit ammunition and are rejected when ammunition is insufficient.
- `GAME_UNIT_RESUPPLY` replenishes a connected unit from the player's season fuel/ammo stores atomically.
- Unit supply state is derived deterministically from the owner's HQ and intact road/infrastructure graph.
- Supply states: `SUPPLIED`, `LOW_SUPPLY`, `ISOLATED`, `OUT_OF_FUEL`, `OUT_OF_AMMO`.
- Destroying a road/depot can therefore isolate a formation immediately; isolated units cannot use `GAME_UNIT_RESUPPLY`.

## Read surface
`qrx generals-unit-logistics-info <chain-dir> <unit-id>`

## Atomicity
Resupply, resource debit, unit fuel/ammo credit, action fee, nonce and applied transaction marker use the existing Generals QRXDB/WAL transaction path.

## Compatibility
No wallet keys, qrx1 addresses, Native Asset IDs, General Unique Assets, existing unit IDs, season IDs, clan IDs, or Phase 1–4.2 state-key identities are changed.

## Deliberate boundary
This phase does not add automatic per-block attrition or starvation damage. Encirclement is represented by deterministic isolation and denial of replenishment. Combat-strength penalties and surrender mechanics can be layered on later without changing the supply graph identity.
