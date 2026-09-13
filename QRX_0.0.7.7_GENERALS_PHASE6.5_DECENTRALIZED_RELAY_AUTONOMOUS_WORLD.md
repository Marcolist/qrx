# QRX Generals Phase 6.5 — Decentralized Command Relay & Autonomous World Resolution

## Network model
QRX Generals does not require game clients or wallets to remain online. The persistent clock requires the QRX validator/node network to keep finalizing blocks. Wallets are only required when a player creates/signs a new command.

## Decentralized signed-command relay
Phase 6.4 scheduled reveal envelopes can now be replicated to ordinary qrxd peers with `GENERALS_RELAY`. Relays validate the embedded signed transaction before accepting it, deduplicate envelopes by SHA3-derived content hash, persist them in `generals-relay`, and admit the exact transaction to VELOCITY mempool when its not-before height is reached. Expired envelopes are never admitted. No wallet key or passphrase is transmitted.

CLI:
- `qrx generals-relay-publish <node-dir> <scheduled-envelope-file>`
- `qrx generals-relay-process <node-dir>`

`qrxd` processes its relay spool while running. Thus the originating PC may go offline after at least one reachable peer has accepted the envelope.

## Autonomous world resolution
Completed-turn pending combat is now consensus-maintained at finalized block boundaries. `finalize-block` scans pending Generals combat records and deterministically applies damage once the recorded turn is older than the current turn, marking the resolution `CONSENSUS_AUTO` and recording the finalized height. No player-signed `GAME_TURN_RESOLVE` transaction is required for this maintenance path.

## Security properties
- relay nodes receive only already-signed transactions
- not-before and expiry windows are immutable envelope metadata
- embedded transactions are signature-verified before persistence
- relays cannot change unit/order/target without invalidating the wallet signature
- autonomous resolution is derived from finalized height + consensus state, not wall-clock time
- no central CURA Generals server is required

## Availability requirement
The game advances only while QRX consensus advances. It is NOT enough that an arbitrary GUI wallet is open. A sufficient live validator set must remain online and reach BFT quorum so new blocks finalize. Non-validating wallets may all be offline.
