# QRX Generals 0.0.7.7 — Phase 4.1 Supply, Production & Strategic Infrastructure

Phase 4.1 extends the serverless Generals consensus state with deterministic logistics inventories, infrastructure construction and unit production. No authoritative game server or SQL database is used.

## Consensus transactions
- GAME_INFRA_BUILD
- GAME_PRODUCE_UNIT
- GAME_SUPPLY_TRANSFER
- GAME_REPAIR_UNIT
- GAME_REARM_UNIT

All use the existing signed Velocity transaction path, nonce/replay protection, ordinary network fee and Generals anti-spam treasury contribution. State mutations are staged in the same QRXDB/WAL batch as the transaction application.

## Logistics
Each season participant starts with deterministic logistics inventory:
- supply: 2000
- fuel: 1000
- ammo: 500
- materials: 1500

These are game resources, not transferable QRX native assets and not QUB. GAME_SUPPLY_TRANSFER can move one resource between active season players. This enables later clan logistics without creating a second off-chain ledger.

## Infrastructure
Infrastructure is chunk-addressed mutable tile state and can only be constructed on territory owned by the signing player. WATER and already occupied infrastructure slots are rejected.

Types and initial material costs:
- FACTORY: 400
- MISSILE_BASE: 700
- AIRFIELD: 600
- ROAD: 40
- SUPPLY_DEPOT: 250
- FORTIFICATION: 180

Infrastructure has deterministic ID, owner, HP, build turn and type. The infrastructure layer does not replace unit tile occupancy, so a base and a unit may logically share a tile.

## Production
GAME_PRODUCE_UNIT consumes supply, fuel and materials based on the consensus unit class and creates a deterministic UNIT-<txid> unit on the first deterministic free adjacent hex.

- AIRFIELD: aircraft classes
- FACTORY: non-air unit classes
- MISSILE_BASE: MISSILE_BATTERY, SAM and MLRS

HQ cannot be produced.

## Service
GAME_REPAIR_UNIT and GAME_REARM_UNIT are consensus primitives for service at an owned SUPPLY_DEPOT. Repair consumes materials; rearm/refuel consumes ammo and fuel.

## Read surface
- `qrx generals-logistics-info <chain> <wallet> <season-id>`
- `qrx generals-infra-info <chain> <season-id> <x> <y>`

## Scope boundary
Phase 4.1 establishes authoritative logistics inventories and strategic infrastructure. Continuous graph-based supply-line connectivity, road movement bonuses, infrastructure combat/destruction and per-shot ammo consumption are intentionally left for the next logistics-hardening increment rather than being simulated off-chain.
