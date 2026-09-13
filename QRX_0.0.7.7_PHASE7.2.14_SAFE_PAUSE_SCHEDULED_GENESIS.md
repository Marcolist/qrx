# QRX 0.0.7.7 Phase 7.2.14 — Safe Pause & Scheduled Genesis

## Mainnet launch
Canonical Mainnet genesis activation is fixed to Tuesday 2026-09-15 18:00 CEST (16:00 UTC), Unix 1789488000. Nodes and wallets may be installed and connected before launch. Mainnet proposal/vote paths fail closed before the activation timestamp.

Genesis memo:
> At the threshold of the AI age, amid global change, QRX was launched to keep value, verification, and digital sovereignty in the hands of people -- and to help ensure that the opportunities of artificial intelligence are open to all, not reserved for a few.

The timestamp, memo, 50 bootstrap validator addresses, governance public keys and other genesis parameters are part of canonical genesis bytes and therefore bind the genesis hash / chain identity.

## Validator Safe Pause
New hybrid-signed consensus transactions: VALIDATOR_PAUSE and VALIDATOR_RESUME. Safe Pause keeps stake bonded but excludes the validator from new liveness expectations/rewards. Existing double-sign evidence remains slashable. Resume does not bypass Phase 7.2.13 catch-up safety: signing resumes only after peer/head/stability gates pass.

Local Validator Mode OFF remains a local runtime switch; use on-chain Safe Pause before planned extended downtime if liveness immunity is desired.
