# QRX Generals 0.0.7.7 — Phase 4
## Deterministic Combat, Unit Classes & Strategic Movement

Phase 4 extends the serverless Generals consensus state without reintroducing an authoritative game server.

### Unit classes
Consensus ships a versioned class table for HQ, Infantry, Mechanized Infantry, Tank, Recon, Artillery, MLRS, SAM, Missile Battery, Helicopter, Fighter, Bomber, Transport and Engineer. Classes define HP, movement points, Energy costs, attack/defense, minimum and maximum range, attack Energy, cooldown, vision and territory-capture capability.

The legacy starter index remains `1 HQ + 3 INFANTRY` for Phase-3 compatibility. Phase 4 adds a separate support detachment (`RECON`, `ARTILLERY`, `MISSILE_BATTERY`) and reports `army_unit_count=7` while keeping `unit_count=4` intact for older consumers.

### Hex geometry and routes
The existing x/y coordinates are interpreted as axial hex coordinates. Legal neighbors are the six axial directions and distance is `max(abs(dx),abs(dy),abs(dx+dy))`.

The Tauri Generals client contains a local A* pathfinder. A player can select a unit and distant target rather than clicking every tile. The UI-generated path is not trusted: Core validates each hex, world bounds, WATER exclusion, occupancy and terrain movement cost.

`MARCH` is committed/revealed with order domain `QRX-GENERALS-ORDER-v2`. Core moves as far as the unit movement budget permits and stores the complete persistent route, cursor and target. `GAME_MARCH_ADVANCE` continues the already-authorized route in a later turn. This removes tile-by-tile UX while keeping each state advance signed and replay protected.

### Long-range attacks
`ATTACK` also uses commit/reveal v2. Ranged units have minimum/maximum range, cooldown and Energy cost. Terrain contributes deterministic defensive modifiers.

Attack reveal does **not** immediately damage the target. It writes turn-scoped pending damage. Multiple attacks against a unit add to the same pending bucket in an order-independent way. On a later turn, `GAME_TURN_RESOLVE` applies that turn's complete pending damage exactly once. This means two units can destroy each other in the same turn; resolution order cannot erase an attack that was already validly revealed.

Destroyed units are removed from tile occupancy. Destroying an HQ marks that season player as defeated.

### Territory
Capturing unit classes write an authoritative owner to the chunked tile state when moving onto a tile. New captures increment `territory_score`. HQ tiles are owned from spawn. Territory therefore uses the same Phase-3.1 chunk model and does not create a separate monolithic map database.

### Commands
- `qrx generals-unit-class-info <TYPE>`
- `qrx generals-combat-info <chain-dir> <season-id> <turn-id> <target-unit-id>`
- existing `generals-unit-info`, `generals-player-army-info` and `generals-tile-info` now expose Phase-4 state as applicable.

### Consensus transaction additions
- existing `GAME_ORDER_COMMIT`
- existing `GAME_ORDER_REVEAL` with `action=MARCH` or `action=ATTACK`
- `GAME_MARCH_ADVANCE`
- `GAME_TURN_RESOLVE`

Legacy Phase-3 MOVE commitments remain accepted with the v1 canonical order string.

### UI
The separate QRX Generals Tauri window has been moved much closer to the original neon / grid / TRON-like visual direction: cyan command-grid lines, dark tactical HUD, neon borders, angular controls and strategic side panels. The old server app itself is not authoritative and was not reintroduced.
