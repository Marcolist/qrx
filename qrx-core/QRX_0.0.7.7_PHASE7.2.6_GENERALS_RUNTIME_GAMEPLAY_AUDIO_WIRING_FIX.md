# QRX 0.0.7.7 Phase 7.2.6 – Generals Runtime Gameplay & Audio Wiring Fix

Phase 7.2.6 hardens the native QRX Generals runtime UX without changing the Generals consensus transaction format.

## Movement runtime

- Own movable units now expose reachable target hexes immediately after selection.
- Reachability mirrors the current Core MOVE admission rule: Manhattan distance <= unit `move_range`, non-water destination, unoccupied destination, HQ excluded.
- Selecting a reachable destination renders a local neon route preview before commit.
- A committed MOVE remains visible locally from the persisted commit/reveal order until reveal.
- During REVEAL the route changes state and `GAME_ORDER_REVEAL` is submitted with the original secret/order data.
- Core already applies the MOVE atomically during successful `GAME_ORDER_REVEAL`; the GUI therefore refreshes immediately after reveal instead of waiting for a later turn-resolve transaction.
- Demo mode now persists resolved demo positions and visibly moves the demo unit rather than snapping back to the original hard-coded coordinates.
- macOS Control-click dispatches the same tactical context menu as right-click.

## Audio runtime

- The original four Generals MP3 tracks remain bundled under `GUIWALLET/src/generals/audio/`.
- Track URLs are resolved from `document.baseURI`, which is safe for the Tauri application asset protocol and for development/browser loading.
- Playback now calls `load()` explicitly, reports media errors, restores volume, updates the MUSIC button state, and keeps playback across track changes.
- The Next Track button is a user gesture and now starts the selected next track even when the previous track was paused.
- All four MP3s were validated with ffprobe and contain non-silent audio.

## Consensus compatibility

No consensus rules, transaction encodings, Genesis inputs, Governance, KYC provider state, Privacy, VELOCITY, QRXDB, BTC SPV, tokenization, or installer target definitions were removed or changed by this phase.

## Packaging

Phase 7.2.6 inherits Phase 7.2.5 packaging behavior:
- macOS ARM64/x64: `.app` plus DMG
- Linux x64/ARM64: DEB plus AppImage
- Windows x64: MSI plus NSIS EXE
- self-contained static OpenSSL release path remains intact.
