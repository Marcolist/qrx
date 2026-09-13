# QRX Generals 0.0.7.7 — Phase 5.3
## Research Facilities, Universities, Espionage & Technology Warfare

Phase 5.3 extends the serverless/block-driven Phase 5.2 research system without changing wallet identity, native-asset identity, season entry accounting, or QUB research acceleration semantics.

### Research infrastructure

New buildable `GAME_INFRA_BUILD` infrastructure types:

- `RESEARCH_LAB`: 650 materials, +5 active research capacity.
- `UNIVERSITY`: 900 materials, +10 active research capacity.

Research capacity stacks with Phase 5.1 industrial capacity when a project starts. Industrial research-time reduction remains capped at 25%; research facilities add up to 30%; the combined non-QUB reduction is capped at 45%.

If a Research Lab or University is destroyed through the existing consensus `GAME_INFRA_ATTACK` path, its active research-capacity contribution is removed atomically from the owner.

### Technology intelligence

New signed consensus transaction: `GAME_TECH_RECON`.

Requirements:
- active player in the current season,
- player-owned RECON unit,
- `ELECTRONIC_WARFARE_I`,
- enemy player with an active research project,
- 8 Energy.

It creates bounded game-rule intelligence containing the currently observed technology, branch, scan height, expiry height and transaction ID.

This is not cryptographic secrecy. QRXDB/blockchain state is replicated and inspectable. Phase 5.3 controls what the game client is permitted to reveal/use as intelligence; a modified node can still inspect raw state.

### Research disruption

New signed consensus transaction: `GAME_RESEARCH_DISRUPT`.

Requirements:
- valid unexpired technology intelligence,
- player-owned RECON,
- `ELECTRONIC_WARFARE_I`,
- enemy active research,
- 100 Supply.

Default disruption adds 300 blocks. Total disruption is capped at 25% of the target project's base duration, so research cannot be locked forever. Counterintelligence halves each disruption event.

Technology is never stolen, transferred or QUB-gated by espionage.

### Counterintelligence

New signed consensus transaction: `GAME_COUNTERINTEL_ACTIVATE`.

Requires at least one active Research Lab or University and consumes 200 Supply + 150 Materials. Default protection lasts 180 blocks and halves research-disruption delay while active.

### CLI read paths

- `generals-research-network-info <chain> <player> <season>`
- `generals-espionage-info <chain> <viewer> <target> <season>`
- existing `generals-research-info` now also reports disruption state.

### Original QRX Generals music

The four MP3 files from the original central-server package are byte-identical in the wallet bundle:

- Digital Crown
- Grid Relay
- Gridfall Charge
- Grid Aurora Drift

Before Phase 5.3 the files had already been copied to the Generals wallet tree but the current wallet page did not instantiate or play them. Phase 5.3 wires the original tracks into the separate Tauri Generals window with play/pause, next-track selection, persisted track selection and browser-policy-compatible playback after a user gesture.

### Validation

- CMake configure/build: PASS
- Phase 5.3 structural/core/music test: PASS
- Phase 5.2 regression: PASS
- Phase 5.1 regression: PASS
- Phase 5 regression: PASS
- Known pre-existing compiler warnings remain.
