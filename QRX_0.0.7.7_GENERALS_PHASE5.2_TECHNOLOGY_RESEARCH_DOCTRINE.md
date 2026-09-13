# QRX Generals 0.0.7.7 — Phase 5.2
## Technology, Research & Military Doctrine

Phase 5.2 adds deterministic, season-scoped research and military doctrine to QRX Generals consensus.

### Consensus operations
- `GAME_RESEARCH_START`
- `GAME_RESEARCH_ACCELERATE`
- `GAME_RESEARCH_COMPLETE`
- `GAME_DOCTRINE_SELECT`

### Research model
Research is block based and serverless. Only one project can be active per player/season. Technologies have branch, prerequisite, base duration, Materials cost and Supply cost. Industrial capacity reduces initial research duration by at most 25%.

Initial branches: Economy, Logistics, Armor, Artillery, Air, Missile, Naval and EW. Tier-II technologies require their Tier-I predecessor.

### QUB acceleration
`GAME_RESEARCH_ACCELERATE` uses the transaction `amount` as the QUB contribution. The contribution is debited atomically from the signing wallet and credited to the Generals treasury plus the active season accounting keys:

- `generals:season:<sid>:treasury_total_atoms`
- `generals:season:<sid>:research_atoms`

Default acceleration rate is 360 blocks per 1 QUB (`generals_research_accel_blocks_per_qub`). Total acceleration is capped at 50% of the project's original effective duration. Contributions that exceed the remaining acceleration allowance or would reduce completion below the next block are rejected rather than partially consumed. There are no QUB-exclusive technologies.

Season entry fees are also categorized under `treasury_total_atoms` and `entry_atoms` going forward while remaining reserved in the existing prize-pool accounting.

### Technology effects
Economy and Logistics research directly feeds the Phase-5 economy collector through research Supply, Fuel and Materials bonuses. Military branches create deterministic player technology modifier state for later/current combat-system consumption.

### Doctrines
A player may select one doctrine per season. Selection costs 500 Supply + 500 Materials and cannot be changed in Phase 5.2:

- `ARMORED_SPEARHEAD`
- `FORTRESS_DEFENSE`
- `AIR_SUPREMACY`
- `DEEP_LOGISTICS`
- `MARITIME_POWER`

The doctrine and deterministic modifier fields are stored in QRXDB consensus state. No doctrine requires QUB.

### Read commands
- `qrx generals-research-info <chain> <player> <season>`
- `qrx generals-tech-tree <chain> <player> <season>`
- `qrx generals-doctrine-info <chain> <player> <season>`

### Compatibility
Phase 5.2 does not change wallet keys, qrx1 addresses, General Native Unique Assets, clan identities, season IDs, or existing Phase 1–5.1 state keys. New state is additive.
