# QRX Generals 0.0.7.7 Phase 4.8 — Naval Warfare, Amphibious Operations & Strategic Transport

Phase 4.8 extends the deterministic Generals consensus model to WATER hexes.

## Naval classes
PATROL_BOAT, DESTROYER, FRIGATE, CRUISER, AMPHIBIOUS_TRANSPORT, SUPPLY_SHIP, CARRIER.

## Infrastructure
PORT and NAVAL_YARD are defined alongside the existing strategic infrastructure classes.

## Consensus transaction family
GAME_NAVAL_DEPLOY, GAME_NAVAL_MOVE, GAME_NAVAL_ATTACK, GAME_AMPHIBIOUS_LOAD, GAME_AMPHIBIOUS_LAND, GAME_SEA_SUPPLY.

The initial executable primitives in this phase cover WATER-only naval movement, single-unit amphibious embark/disembark, and sea resupply. Naval deploy/attack identifiers are consensus-reserved for the production/combat integration follow-up; they are deliberately rejected until their full atomic implementation is enabled rather than accepting incomplete state transitions.

## Amphibious rules
An AMPHIBIOUS_TRANSPORT may embark one friendly non-naval unit from an adjacent coastal land hex. The embarked unit is removed from tile occupancy and marked embarked. It can later disembark to a free adjacent non-WATER hex and becomes active again.

## Strategic transport
SUPPLY_SHIP can replenish a friendly unit within two hexes by atomically drawing ammo/fuel from the owner's season logistics pool.

## Compatibility
No existing wallet/key/address, General unique asset, clan, season, land unit, air mission or Phase 1–4.7 state-key identity is changed.
