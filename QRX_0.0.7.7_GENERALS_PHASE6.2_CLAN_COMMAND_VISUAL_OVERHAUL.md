# QRX Generals 0.0.7.7 — Phase 6.2

## Clan Command Authority & Visual Overhaul

Phase 6.2 turns the clan leader into an actual command role without giving the leader custody over clan funds.

### Leader / officer authority

New consensus transaction types:

- `GAME_CLAN_OFFICER_SET`
- `GAME_CLAN_DIRECTIVE`
- `GAME_CLAN_LEADER_TRANSFER`

The leader can appoint up to three officers. Leader and officers may invite players and place a current-season tactical directive on the map (`ATTACK`, `DEFEND`, `RALLY`, `SUPPLY`, `RECON`). Directives are authoritative QRXDB state and are shown as a pulsing marker in the Generals map. The leader can transfer leadership to an active clan member.

### Commander reward

The Season reward split remains 70% Top-20 players / 30% Top-3 clans. For a Top-3 clan, 5% of that clan's award is assigned directly to the leader snapshotted when the clan first joins the season, if that leader participates in the season. The remaining 95% of the clan award is still distributed among season members proportionally to score.

This means the leader has a visible incentive to organize a successful clan, but cannot seize, redirect or custody the clan reward pool. A mid-season leadership transfer does not rewrite the season reward snapshot.

### Visual overhaul

The Generals Tauri window now uses a deeper galaxy/neon command aesthetic: animated star depth, brighter TRON-like cyan hex grid, magenta clan-command overlays, gold selection/command accents, sharper HUD panels, beveled map frame, stronger unit/contact contrast, and redesigned command cards/context menus. The map remains deliberately simple and readable rather than becoming a heavy 3D renderer.

Right-clicking a hex as a leader/officer adds Clan Command actions for Attack, Defend, Rally and Supply directives.

### Validation

- Fresh CMake Core build: PASS
- JavaScript syntax: PASS
- Phase 6.2 behavioral test: PASS
- All 19 `generals*.sh` tests: PASS when supplied the absolute Phase 6.2 qrx binary path
- Existing Phase 6.1 reward accounting regression: PASS
- ZIP integrity: performed at packaging

Native Tauri installers are not built in the assistant container when a Rust/Cargo toolchain is unavailable.
