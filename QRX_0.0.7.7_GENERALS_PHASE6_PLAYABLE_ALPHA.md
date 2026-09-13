# QRX Generals 0.0.7.7 — Phase 6: Playable Alpha Completion

Phase 6 stops adding isolated rule primitives and connects the existing Generals consensus game to the QRX GUI Wallet as a playable Alpha path.

## Playable loop

The Generals Tauri window now has a real QRX-Core bridge and a neon tactical SVG hex map. The wallet is the player identity. From the GUI an Alpha player can create the on-chain General, join the active season, inspect the local deterministic world region and army, select units/hexes, commit and reveal MOVE orders, collect economy output, build infrastructure, start/complete research, optionally accelerate active research with capped QUB funding into the Season Treasury, select a doctrine, finalize an ended season and claim a winning QUB reward.

The map is intentionally simple and readable: dark space background, cyan neon hexes, holographic HUD frames, restrained magenta/gold accents, and small tactical unit markers. It follows the atmosphere of the original QRX Generals web version while moving toward a generic TRON-like / galactic-war command-console feeling rather than cloning a third-party game interface.

The four original web-version music files remain bundled and are used by the Generals window.

## Consensus additions

New transaction types:

- `GAME_SEASON_FINALIZE`
- `GAME_REWARD_CLAIM`

New read commands:

- `generals-region-info <chain> <season> <x0> <y0> <w> <h>`
- `generals-ranking-info <chain> <season>`
- `generals-season-result-info <chain> <season>`

Season joins now maintain a deterministic bounded participant index. Finalization is permissionless after the season end: any valid signed wallet transaction can trigger it, so there is no central game server or operator timer. Alpha scoring combines territory, strategic, economy and development scores plus a survival bonus. Equal scores are resolved deterministically by wallet-address ordering.

The Phase-6 Alpha reward policy is winner-takes-all. Finalization converts the existing reserved Season Prize Pool into a claimable reward for the deterministic winner. `GAME_REWARD_CLAIM` atomically marks the reward claimed, reduces Generals treasury/reserved state and credits authoritative QUB balance in the same QRXDB batch. Replays and double claims are rejected.

## GUI / Core bridge

New Tauri commands:

- `generals_snapshot`
- `generals_submit_action`
- `generals_order_commitment`

`generals_snapshot` aggregates world, season, wallet-player identity, treasury, army, unit, logistics, economy, research, doctrine, ranking, previous-season result and a bounded tactical region. `generals_submit_action` only accepts `GAME_*` actions, signs with the selected QRX wallet and broadcasts through the existing local Core/CLI path. The passphrase is not written to Generals state or blockchain. `generals_order_commitment` computes the SHA3-512 commit/reveal commitment used by MOVE orders.

## Audit fix included

The previously found `gov_protocol_propose_cmd()` format-string bug is fixed: the generated governance proposal now supplies the nonce argument corresponding to `nonce=%s` instead of invoking undefined behavior.

The old Phase-4 regression was updated for the Phase-4.4+ visibility rule. Blind long-range missile fire must now fail without a valid recon/radar contact.

## Validation

- Fresh CMake Core build: PASS
- Generals regression scripts Phase 1 through 5.3 currently present in the tree: PASS
- Phase 6 behavioral lifecycle test: PASS
- Season finalization: PASS
- Atomic QUB reward claim: PASS
- Reward replay rejection: PASS
- Region/map API: PASS
- GUI JavaScript syntax (`node --check`): PASS
- ZIP integrity: performed at packaging

Native Tauri compilation is **not** validated in this build environment because `cargo`/Rust toolchain is unavailable here. Core compiler warnings inherited from the broader 0.0.7 tree remain; this is not a warning-free or independently audited Mainnet build.

## Alpha limitations

Phase 6 is a playable **Alpha completion**, not a Mainnet certification. The basic player/season/map/movement/economy/research/reward loop is wired to the GUI. Many advanced air, naval, espionage and combat primitives from Phases 4.x–5.3 still need dedicated graphical controls and richer animations. Some advanced resolve operations remain explicit permissionless transactions rather than automatic block hooks. Fog of War is a gameplay-rule visibility layer over replicated QRXDB state, not cryptographic coordinate secrecy.

Paid real-QUB competitive seasons should not be enabled on Mainnet before independent consensus/security review and legal review of the entry-fee/prize structure.
