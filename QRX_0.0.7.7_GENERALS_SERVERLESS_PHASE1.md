# QRX 0.0.7.7 — Generals Serverless Migration Phase 1

## Scope

Phase 1 moves the first authoritative QRX Generals objects out of the historical web-server/database model and into deterministic QRX consensus state backed by QRXDB + WAL.

Implemented consensus transaction types:

- `GAME_JOIN`
- `GAME_CLAN_CREATE`
- `GAME_TREASURY_FUND`

Implemented read-only CLI views:

- `generals-player-info <chain-dir> <qrx-address>`
- `generals-clan-info <chain-dir> <clan-id>`
- `generals-treasury-info <chain-dir>`

Transactions use the existing VELOCITY v2 canonical transaction format, network/genesis/protocol binding, hybrid wallet signatures, lane nonces, replay protection and the same atomic QRXDB WAL batch as the QUB account debit, fee accounting and transaction index.

## Wallet account / Player Identity

`GAME_JOIN` is signed by the player's QRX wallet. Phase 1 derives a deterministic player id from the signing wallet address using a domain-separated SHA3-512 digest:

`QRX-GENERALS-PLAYER-v1 | qrx-address`

No username/password account service exists in the consensus path. One signing wallet can create only one Phase-1 player identity.

State keys include:

- `generals:player:<wallet>:status`
- `generals:player:<wallet>:player_id`
- `generals:player:<wallet>:general_asset`
- `generals:player:<wallet>:joined_height`
- `generals:player:<wallet>:join_tx`
- `generals:player:<wallet>:clan_id`
- `generals:player_id:<player-id>`

## General = native UNIQUE asset

`GAME_JOIN` atomically creates exactly one native asset named `GENERAL#<deterministic-id>`.

Consensus metadata:

- `kind=UNIQUE`
- `supply=1`
- `max_supply=1`
- `units=0`
- `reissuable=0`
- `issuer=QRX_GENERALS_PROTOCOL`
- `metadata=qrx-generals-general-v1`
- `capabilities=GAME_GENERAL_V1`

The asset balance is credited to the joining QRX wallet in the existing native-asset balance namespace.

Generic `ASSET_TRANSFER` deliberately rejects `GENERAL#...` assets in Phase 1. A later `GAME_GENERAL_TRANSFER` must update both native-asset ownership and Generals ownership atomically; this prevents the game state and asset state from diverging.

## GAME_JOIN and treasury

Default consensus cost:

- `generals_join_cost_atoms = 1,000,000,000` = **10 QUB**

The cost is not a normal transaction fee and is not burned. It is debited from the signing wallet and credited to the protocol-owned Generals treasury in the same WAL transaction.

The value is a chain parameter (`generals_join_cost_atoms`) and can be fixed/activated by the chain parameter schedule before mainnet launch.

## Clan creation

`GAME_CLAN_CREATE` requires an active Generals player.

Payload:

`clan_name=<3..32 safe chars>;clan_tag=<2..6 alphanumeric/_/->`

Phase 1 enforces:

- globally unique clan name;
- globally unique clan tag;
- one clan membership for the founder at creation time;
- founder is initial leader and first member;
- deterministic clan id derived from the committed transaction id;
- all clan registry/index/membership writes are in the same QRXDB WAL batch.

Default consensus cost:

- `generals_clan_create_cost_atoms = 5,000,000,000` = **50 QUB**

The complete amount is credited to the Generals treasury.

## On-chain Generals Treasury

The authoritative treasury is consensus state, not a wallet controlled by CURA or a game server:

- `generals:treasury:balance_atoms`
- `generals:treasury:last_height`
- `generals:treasury:last_tx`
- `generals:treasury:last_source`

Phase 1 can increase it by:

- `GAME_JOIN`;
- `GAME_CLAN_CREATE`;
- voluntary `GAME_TREASURY_FUND` transactions.

`GAME_TREASURY_FUND` uses the transaction `amount` as its treasury contribution. There is intentionally **no treasury withdrawal or reward payout transaction in Phase 1**. This means Phase 1 cannot drain the treasury; payout/season accounting will be introduced with explicit consensus rules in a later phase.

## Atomicity

For every Phase-1 game transaction, one QRXDB batch covers:

1. player's QUB debit;
2. normal network fee accounting;
3. game state mutation;
4. treasury credit;
5. native General asset creation/balance (for `GAME_JOIN`);
6. lane nonce;
7. applied-transaction marker;
8. transaction index.

If any game rule fails, the batch is aborted. A failed duplicate `GAME_JOIN`, invalid clan creation, or invalid General state therefore cannot consume the game contribution or partially create state.

## Activation / chain parameters

`getparams` now exposes:

- `generals_activation_height`
- `generals_join_cost_atoms`
- `generals_clan_create_cost_atoms`
- `generals_account_model=wallet-signed-player-identity`
- `generals_state_backend=qrxdb-wal-consensus`

The transaction family is rejected before `generals_activation_height`.

## Phase-1 boundaries

Not implemented yet, deliberately:

- map / territory state;
- turns and block-height round windows;
- energy;
- movement / combat;
- commit/reveal orders;
- season registration and season prize accounting;
- booster purchases;
- reward claims / verified payout policy;
- clan invitations, joining, leaving and leadership transfer;
- General transfer (`GAME_GENERAL_TRANSFER`).

These require their own deterministic state transitions and tests; they must not be simulated by the old server database if the target is a genuinely serverless game.
