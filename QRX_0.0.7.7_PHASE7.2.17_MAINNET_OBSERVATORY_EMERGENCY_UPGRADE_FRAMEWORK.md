# QRX 0.0.7.7 Phase 7.2.17 — Mainnet Observatory & Emergency Upgrade Framework

## Purpose
Operational launch guardrails, not a replacement for consensus testing. Phase 7.2.17 adds a local authenticated health RPC, a multi-node convergence watcher, a peer endpoint viewer and a fail-closed activation-height readiness check for emergency upgrades.

## `getmainnethealth`
`qrx-cli --network mainnet getmainnethealth` combines current height/hash, peer height, connection count, QRXDB state root output, protocol-upgrade status and the authoritative supply invariant. It deliberately does not expose RPC tokens or wallet secrets.

## Observatory
`scripts/qrx-mainnet-observatory.py --config config/mainnet-observatory.example.json`

The watcher queries multiple independently configured local nodes and writes atomic `mainnet-health.json` suitable for qrxscan ingestion. It raises CRITICAL if nodes report different block hashes/state-root payloads at the same height or if the supply invariant fails. RED is used for required protocol upgrades or loss of a configured observer quorum; YELLOW covers lag/no-peer conditions.

For production, run observer nodes on separate hosts/networks and aggregate their signed/transport-authenticated reports. The example deliberately uses local datadirs so a node RPC token is not transmitted across an unencrypted network.

## Peer Viewer
Yes, a peer viewer is useful. `scripts/qrx-peer-viewer.py <node-dir> --resolve` shows peer source (`peers.txt`, `known_peers.txt`, `seeds.txt`), domain/IP, port, endpoint type/scope and locally resolved IPs.

Do **not** publish the entire peer/validator IP list on qrxscan by default. Publicly exposing validator endpoints makes targeted DDoS easier. qrxscan should show aggregate peer count, diversity metrics and public seed domains; detailed IP/domain rows should stay in authenticated operator/admin views unless an operator intentionally advertises a public relay.

## Emergency upgrades
QRX already has developer-threshold `PROTOCOL_UPGRADE` governance with activation heights. Phase 7.2.17 does not introduce a central kill switch and does not make wallet update timing itself a consensus rule. An emergency build must be distributed first, then activated deterministically at an agreed future block height.

`scripts/qrx-emergency-upgrade-check.py` fails closed unless activation has sufficient lead time and observed upgraded voting power is strictly >2/3. Production release policy can use a higher target (recommended 80–90% before activation) even though BFT safety threshold remains >2/3.

## Suggested operational alarms
GREEN: observers converge, supply invariant passes, no protocol update required.
YELLOW: node >2 blocks behind or no peers.
RED: observer quorum unavailable or mandatory protocol update pending.
CRITICAL: conflicting same-height block hash/state root, failed supply invariant, or confirmed incompatible finality.

On CRITICAL: do not automatically rewrite state or roll back the chain. Alert operators, investigate independently, Safe Pause validators if needed, publish signed incident information, and use deterministic upgrade activation if a consensus fix is required.
