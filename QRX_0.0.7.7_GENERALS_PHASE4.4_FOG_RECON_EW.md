# QRX Generals 0.0.7.7 — Phase 4.4: Fog of War, Reconnaissance & Electronic Warfare

Phase 4.4 adds consensus-enforced game knowledge and targeting rules to the serverless Generals state machine.

## New signed transitions
- `GAME_RECON_SCAN`: an eligible RECON/FIGHTER (or other vision-capable unit at normal range) records a time-limited contact for an enemy unit.
- `GAME_EW_JAM`: RECON/FIGHTER EW mission applies a time-limited jam state to a target player.

## Visibility and contacts
A target is game-visible when it is inside effective vision of one of the viewer's active indexed units, or a still-valid reconnaissance contact exists. Contacts store last-seen height and coordinates and expire deterministically. Default contact TTL is 120 blocks.

Jamming is consensus state. While jammed, effective direct vision is reduced by half (minimum one hex). Default jam TTL is 60 blocks. Long-range unit attack reveal now requires game visibility/contact; range alone is not sufficient.

## Critical privacy boundary
QRXDB/blockchain state is replicated/public. Therefore Phase 4.4 is **game-rule Fog of War**, not cryptographic concealment of raw blockchain data. A modified client or node operator can inspect authoritative unit coordinates. The official GUI must only render information allowed by the visibility/contact rules. True cryptographic hidden positions would require a later private-state/proof architecture and is intentionally not claimed here.

## Determinism
All contact expiry, jamming expiry, vision calculations, hex distance, and targeting authorization depend only on consensus state and block height. No game server, wall-clock timer, or operator oracle is authoritative.
