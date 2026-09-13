# QRX Generals 0.0.7.7 — Phase 4.2 Supply Lines, Roads & Infrastructure Warfare

Phase 4.2 extends the deterministic QRXDB/WAL game state with road-linked supply reachability and destructible strategic infrastructure.

## Consensus transactions
- `GAME_INFRA_ATTACK`: a player-owned combat unit may attack enemy infrastructure within its unit-class range. Damage is deterministic from unit attack and infrastructure defense. HP/status/last attack transaction are committed atomically; zero HP marks the infrastructure destroyed.
- `GAME_ROAD_REPAIR`: repairs a player-owned damaged ROAD to full HP and atomically consumes Materials.

Both use the existing signed Velocity transaction path, nonce/replay protection, ordinary network fee, Generals action anti-spam cost, QRXDB WAL batch and applied-transaction marker.

## Supply network
`generals-supply-line-info` performs a deterministic bounded hex BFS across intact, player-owned ROAD / SUPPLY_DEPOT / FACTORY / AIRFIELD / MISSILE_BASE nodes. Destroying a road or node (HP=0) breaks that route immediately. The graph is derived from authoritative chunked QRXDB state; no game server or SQL database participates.

The bounded traversal prevents an unbounded consensus/read workload. Phase 4.2 intentionally provides supply connectivity as the primitive. Automatic per-turn attrition and full per-shot ammunition/fuel consumption remain for the next logistics-balancing phase rather than being silently approximated.

## Infrastructure state
Infrastructure info now exposes `status`, `last_attack_tx`, and `destroyed_height` in addition to id/type/owner/hp/built_turn.

## Compatibility
No wallet keys, qrx1 addresses, General UNIQUE asset identity, native asset semantics, season identity, existing unit IDs, or previous Generals state keys are changed.
