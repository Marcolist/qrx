# QRX Generals Phase 6.4 — Persistent World & Offline Command Execution

## Implemented
- QRX block height remains the authoritative world clock; GUI presence is irrelevant.
- MOVE commit now prepares a separately signed `GAME_ORDER_REVEAL` envelope while the wallet is available.
- The signed envelope is stored in `generals-offline-queue` with a strict `not_before_height` (start of REVEAL) and `expires_height` (turn end).
- `qrxd` maintenance invokes `generals-offline-process` every maintenance cycle.
- The relay has no wallet/private keys and cannot alter the order; it can only admit the exact pre-signed transaction.
- Expired envelopes are quarantined as `.expired`; admitted envelopes become `.relayed`.
- Manual reveal remains available as fallback.

## Security model
This is pre-authorization, not key delegation. The node never receives a passphrase or private key. Authorization is bounded by the signed transaction payload, network/chain replay protection, transaction expiry, unit/order/season/turn checks and the existing commit hash.

## Persistence semantics
Research, seasons, turn windows and other height-derived state continue whenever QRXChain produces blocks. A queued order can reveal while the Generals GUI is closed as long as a local `qrxd` holding the spool is running. For execution while the player's entire machine/node is offline, the signed spool must additionally be handed to an external QRX relay before disconnect; Phase 6.4 deliberately does not upload player orders to a central CURA server.

## Validation
- Phase 6.4 wiring test: PASS.
- Fresh CMake build of qrx/qrx-cli/qrxd with Phase 6.4 source: PASS.
- Existing source tree retained; no Generals consensus action removed.
