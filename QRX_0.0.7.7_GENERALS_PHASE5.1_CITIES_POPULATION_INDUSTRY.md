# QRX Generals 0.0.7.7 — Phase 5.1
## Cities, Population, Industry & Economic Development

Phase 5.1 extends the deterministic Phase 5 economy without changing QRX wallet/key/address identities.

### Consensus transactions
- `GAME_CITY_DEVELOP` — upgrades an owned deterministic CITY from level 1 up to level 5. Development consumes Supply + Materials atomically, increases population and industry, adds persistent production bonuses and development score.
- `GAME_INDUSTRY_INVEST` — increases industry (max 10), consumes Materials atomically and increases industrial capacity/material production bonus.

### City state
Each deterministic CITY has lazy consensus state under `generals:season:<sid>:city:<x>:<y>:`. Defaults are level 1, population 1000, industry 1. Development records the transaction/turn. Ownership remains the Phase 5 strategic-objective owner.

### Economic effect
City development adds +25 Supply and +20 Materials per economy collection for each successful level upgrade. Industry investment adds +30 Materials per economy collection and +1 industrial capacity. Existing base Phase 5 CITY/OIL/MINE/SUPPLY_HUB/COMMAND_CENTER production remains intact.

### Read surface
`qrx generals-city-info <chain-dir> <season-id> <x> <y>`

`generals-economy-info` additionally reports `development_score` and `industrial_capacity`.

### Atomicity
Resource debit, city/industry state, economic bonuses, score, nonce/applied transaction bookkeeping and normal Generals action treasury cost remain in the signed transaction's QRXDB/WAL commit path.
