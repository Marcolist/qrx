# QRX Generals 0.0.7.7 — Phase 4.9
## Fleets, Carrier Operations & Naval Combat Resolution

Phase 4.9 extends the serverless Generals consensus state with operational naval production and fleet warfare.

Consensus transaction family:
- `GAME_NAVAL_DEPLOY`: produces a naval unit at an owned `NAVAL_YARD`, charges game resources and spawns it on adjacent WATER.
- `GAME_FLEET_CREATE`: groups 2–16 owned naval units into a deterministic `FLEET-*` state.
- `GAME_NAVAL_ATTACK`: validates ownership, naval classes, weapon range and ammunition, then stages turn-scoped pending damage.
- `GAME_NAVAL_COMBAT_RESOLVE`: applies accumulated naval damage in a later turn, allowing simultaneous destruction rather than transaction-order wins.
- `GAME_CARRIER_AIR_WING`: associates owned Fighter/Helicopter units with an adjacent owned Carrier.
- `GAME_NAVAL_BLOCKADE`: establishes a short block-height bounded blockade marker around a naval unit.

Existing Phase 4.8 movement, amphibious landing and sea supply remain compatible. Naval combat follows the same deterministic delayed-resolution principle introduced for land combat: attacks stage damage, a later resolution transition applies it. Destroyed ships are removed from tile occupancy.

Carrier air-wing state is intentionally a foundation rather than a claim of a complete carrier flight-deck simulation. Future work can add launch/recovery capacity, sortie cycles, anti-ship missile missions and layered fleet air defence without changing fleet identity.
