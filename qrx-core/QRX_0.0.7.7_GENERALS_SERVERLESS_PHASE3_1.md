# QRX Generals 0.0.7.7 — Phase 3.1
## Adaptive Seasonal World Size + Chunking + Fair HQ Spawn

Phase 3.1 replaces the Phase-3 fixed 128x128 world assumption with a deterministic, monotonically expanding seasonal world.

### Adaptive world tiers

The authoritative season world grows according to actual season participation:

- 1–100 players: 128x128
- 101–500 players: 256x256
- 501–2,000 players: 512x512
- 2,001–8,000 players: 1024x1024
- >8,000 players: 2048x2048 initially, then deterministic power-of-two growth as needed

World dimensions are persisted under the season state and can only grow, never shrink. Existing coordinates therefore remain valid when a later join crosses a tier boundary.

### 64x64 chunked occupancy

Terrain remains deterministic from season_id/x/y and is not materialized tile-by-tile. Mutable occupancy is stored with chunk-local keys:

`generals:season:<sid>:chunk:<cx>:<cy>:tile:<lx>:<ly>:unit`

The default chunk size is 64. Phase-3 flat tile keys remain read-compatible for migration, but new writes use chunked occupancy.

### Fair HQ spawning

HQ placement is deterministic but now enforces a default Manhattan separation of 16 tiles between HQs. Candidate HQ tiles must be non-water, unoccupied, inside safe map edges, and have enough nearby valid tiles for the three starter infantry units.

Starter army layout is now clustered around the HQ instead of scattering all four starter units independently across the map.

Season state keeps a deterministic HQ index:

- `generals:season:<sid>:hq_count`
- `generals:season:<sid>:hq:<n>:x`
- `generals:season:<sid>:hq:<n>:y`
- `generals:season:<sid>:hq:<n>:owner`

### Introspection

`qrx generals-world-info <chain> [height]` now reports world dimensions, chunk size/count, HQ minimum distance and season player count.

`qrx generals-world-plan <players>` reports the deterministic size tier without mutating chain state.

`qrx generals-tile-info` reports global and chunk-local coordinates.

### Compatibility

Existing QRX wallet addresses, keys, General UNIQUE assets, Phase-1/2 game state, commit/reveal order semantics and native asset consensus are unchanged.

### Next phase

Phase 4 is named **Deterministic Combat & Territory**. It will build attacks on occupied tiles, deterministic simultaneous conflict resolution, unit combat attributes, territory capture, HQ defeat rules and initial season scoring on top of the Phase-3.1 world model.
