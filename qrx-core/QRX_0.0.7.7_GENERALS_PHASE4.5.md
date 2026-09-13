# QRX Generals 0.0.7.7 Phase 4.5 — Air Power, Radar, SAM & Strategic Missile Warfare

Phase 4.5 extends the deterministic QRXDB/WAL game state with mission objects instead of treating all long-range warfare as instantaneous damage.

## Consensus transactions
- GAME_AIR_MISSION — launches FIGHTER/BOMBER/HELICOPTER/TRANSPORT missions to map coordinates; fuel is charged and the mission has a block-height resolve time.
- GAME_RADAR_SCAN — RADAR_STATION creates time-limited contacts within its deterministic radius.
- GAME_MISSILE_LAUNCH — MISSILE_BATTERY launches an in-flight strategic missile against a currently valid contact; ammunition is charged at launch.
- GAME_SAM_INTERCEPT — SAM units can consume ammunition to mark an enemy in-flight mission intercepted before resolution.
- GAME_STRIKE_RESOLVE — resolves a matured, non-intercepted mission exactly once.

## Infrastructure
RADAR_STATION and SAM_SITE are now valid infrastructure classes in addition to the Phase 4.1/4.2 infrastructure set.

## Mission state
Mission IDs are deterministic from the signed transaction id (`AIR-...` / `MSL-...`). State includes owner, source unit, target, launch height, resolve height and status (`in_flight`, `intercepted`, `resolved`).

## Security / determinism
All writes use the existing signed Velocity transaction path and QRXDB WAL batch. Radar contacts integrate with Phase 4.4 fog-of-war rules. Strategic missiles require a valid contact. No wall-clock timers or central game server are used.

## Scope boundary
This phase establishes deterministic air/missile mission and interception primitives. Detailed probabilistic air-combat simulation, airbase sortie queues, radar line-of-sight terrain models, multiple interceptor salvos and balance tuning remain later work.
