# QRX Generals 0.0.7.7 — Phase 6.1

## Full GUI Gameplay Wiring & Battle Presentation

Phase 6.1 turns the Phase-6 playable-alpha shell into a tactical command surface. The neon hex world keeps the intentionally simple TRON / galactic-war visual language while adding context-sensitive right-click orders, live panels for economy/research/doctrine/season/treasury/ranking/clans, unit quick actions, battle-feed feedback and the original four music tracks.

### Right-click tactical menu

The menu adapts to the selected own unit, target tile and contact. Wired actions include movement commit, resupply, repair/rearm, recon, EW jam, air strike, CAP, SEAD, RTB, naval movement/attack, infrastructure construction, strategic capture, city development and industry investment. Consensus validation remains authoritative; the GUI cannot bypass Core rules.

### Clan purpose

Clans are no longer only social grouping. They provide coordinated play, common season identity and a separate competitive reward path. Clan membership is snapshotted when the player joins a season, preventing last-minute clan hopping to capture a payout.

### Season reward policy

The full reserved season prize pool is distributed deterministically:

- 70%: top 20 players, descending linear weights.
- 30%: top 3 clans, ranked by the sum of their season members' scores.
- Clan pool: 50/30/20 between the top three clans (normalized if fewer qualify).
- Each winning clan's award is distributed to its season members proportionally to individual score, with a minimum weight of 1.
- A player can therefore earn both an individual ranking reward and a clan contribution reward.
- If no clan qualifies, the clan pool falls back to the top player so the complete prize pool remains payable.
- All rounding remainder is assigned deterministically; reward_total_atoms equals prize_pool_atoms.

New read commands:

- `generals-season-rewards-info <chain> <season>`
- `generals-clan-ranking-info <chain> <season>`

The existing `GAME_REWARD_CLAIM` remains per-wallet and releases only that wallet's reserved amount.

### Validation

Fresh CMake Core build: PASS.
All 18 current `generals*.sh` tests against the Phase-6.1 Core: PASS.
Phase 6.1 behavioral reward/clan test: PASS.
Phase 6 lifecycle regression: PASS.
GUI JavaScript syntax (`node --check`): PASS.
Native Tauri build: not run because Cargo/Rust is unavailable in the build environment.
