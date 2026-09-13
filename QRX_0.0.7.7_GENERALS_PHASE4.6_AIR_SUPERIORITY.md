# QRX Generals 0.0.7.7 — Phase 4.6 Air Superiority, Interception & Mission Resolution

Adds signed consensus transitions GAME_CAP_MISSION, GAME_ESCORT_MISSION, GAME_AIR_INTERCEPT and GAME_AIR_COMBAT_RESOLVE on top of Phase 4.5.

* Fighter CAP creates a block-height bounded air-defense patrol zone.
* Fighter escort attaches to an in-flight friendly air/strike mission.
* Fighter interception consumes operational fuel/ammunition and records an engagement against an enemy in-flight mission.
* Air-combat resolution is deterministic and records attack/defense strength and either defender_wins/intercepted or attacker_breakthrough/repelled.
* Existing Phase 4.5 SAM interception remains available as the ground-based layer; this phase does not replace it.
* No random/block-order-dependent combat outcome is introduced.

Follow-up balancing should add explicit CAP auto-discovery, radar-linked engagement envelopes, aircraft damage/RTB state, multi-interceptor formations and escort attrition.
