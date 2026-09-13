# QRX Generals 0.0.7.7 Phase 4.7 — Advanced Air Operations, Formations & Air Defense Network

Phase 4.7 extends the deterministic QRXDB/WAL Generals state machine with signed consensus primitives for multi-aircraft formations, explicit CAP auto-interception, return-to-base state and SEAD missions.

New transaction types:
- GAME_AIR_FORMATION — creates FORM-* consensus objects with 2–8 owned air units.
- GAME_CAP_AUTO_INTERCEPT — lets an active CAP zone explicitly intercept an in-flight enemy mission inside its radius. It is a signed state transition, never a hidden server timer.
- GAME_AIR_RTB — marks owned aircraft as returning-to-base in authoritative state.
- GAME_SEAD_MISSION — launches a Fighter/Bomber suppression mission against a map coordinate and consumes fuel/ammunition.

The existing Phase 4.5/4.6 mission, radar, SAM, escort and deterministic air-combat states remain intact. Phase 4.7 deliberately does not introduce random combat outcomes or an off-chain authoritative scheduler.

Security/consensus invariant: GUI automation may propose CAP responses or RTB actions, but QRX Core validates and commits them. No central Generals server is authoritative.
