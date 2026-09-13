# QRX 0.0.7.7 Phase 7.2.7 — Generals Turn Controller & Commit/Reveal UX

Phase 7.2.7 makes the Generals commit/reveal lifecycle explicit and testable.

## Runtime behavior

- The HUD now shows a prominent COMMIT or REVEAL state with a progress bar and countdown.
- Main-network phase authority remains consensus-derived. The GUI uses `height`, `turn_start_height`, `turn_end_height`, `order_phase` and the configured/default block time only to estimate time remaining; it never locally overrides consensus phase.
- The snapshot is refreshed every 5 seconds and aggressively refreshed at an estimated phase boundary.
- Sandbox uses an accelerated 60-second turn: 30 seconds COMMIT + 30 seconds REVEAL so gameplay can be tested quickly.
- Quick-command buttons are phase aware. COMMIT is disabled in REVEAL; REVEAL is disabled in COMMIT or when no valid local committed order exists.
- A locally stored reveal is tied to its turn ID. Stale/unrevealed local orders from an earlier turn are rejected/expired instead of being revealed in a later turn.
- A second MOVE commit in the same turn is blocked in the UI.
- The order queue explicitly says whether the order is hidden/committed or ready to reveal.
- Phase transitions generate a visible toast/battle-feed notification.

## Consensus relationship

Core remains authoritative. QRX Generals currently derives turns from `generals_turn_length_blocks` (default 60 blocks). `generals_order_phase()` splits each turn into COMMIT and REVEAL halves. `GAME_ORDER_COMMIT` is rejected outside COMMIT and `GAME_ORDER_REVEAL` is rejected outside REVEAL. Phase 7.2.7 mirrors these existing consensus rules in the desktop UX rather than introducing a second game clock.

## Regression preservation

Phase 7.2.6 movement preview/reachability/reveal-map refresh and Tauri-safe music playback remain present. Phase 7.2.5 cross-platform installer packaging remains unchanged.
