# QRX 0.0.7.7 Phase 7.2.16 — Mainnet Genesis Dress Rehearsal / Byzantine Multi-Node Simulation

Phase 7.2.16 is a release-gate test phase built on 7.2.15. It intentionally adds no new Mainnet consensus feature and no test-only Mainnet clock/network bypass.

Scheduled Mainnet activation remains 2026-09-15 18:00 CEST (16:00 UTC; Unix 1789488000).

## Adversarial rehearsal topology

The deterministic harness models seven independently validating nodes with 100 units of weighted validator power. It exercises the same release invariants introduced in Phase 7.2.15: exact finalized parent, exact next height, parent state-root binding, deterministic weighted proposer, validator eligibility, scheduled Genesis time, timestamp bounds, and strict >2/3 finality.

Scenarios include: pre-Genesis rejection; seven-node happy-path finality; wrong parent; height skip; wrong parent state root; wrong proposer; excessive future timestamp; SAFE PAUSED proposer; jailed/tombstoned proposer; strict 2/3 boundary; minority network partition followed by heal; competing blocks and double-vote evidence; crash/restart/catch-up convergence; and 5,000 deterministic randomized quorum/partition cases.

## Why the simulator is separate

The rehearsal does not introduce environment variables or RPC commands capable of overriding Mainnet time, validator eligibility, or finality. Such hooks would themselves become release attack surface. Instead, the simulator is source-bound to production validation markers and models the Byzantine state machine deterministically.

## Run

```bash
./scripts/run-mainnet-genesis-dress-rehearsal.sh
```

For launch, this simulator is one gate only. The final 50 public bootstrap addresses and five governance public keys, real seed DNS, native binaries, and a real multi-host pre-Genesis deployment still require operational verification before the scheduled launch.
