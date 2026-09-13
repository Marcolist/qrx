# QRX 0.0.9.27 — Mainnet PoUC Activation Gate

Implemented in this milestone:

- Consensus-facing PoUC settlement decision layer bound to `chain_id`, `genesis_hash`, and `protocol_version`.
- Explicit activation-readiness gate covering every prerequisite listed in the 0.0.9.27 roadmap: stable verification, economic simulation, FastTrack fairness, standard-task starvation protection, transparent FastTrack development share, model integrity, provider-fraud controls, Resource Globe privacy, and operation without official QRX servers.
- Settlement outcomes: `PAYOUT`, `FROZEN`, `REJECTED`.
- Accepted redundant verification can release verified compute payment through the existing compute escrow settlement function.
- Challenge-required work remains frozen until a challenge is finalized and passed.
- Failed challenges or rejected verification cannot pay out.
- Adversarial evidence carrying `REJECT_RESULT`, `FREEZE_PAYOUT`, or `SLASH_CANDIDATE` feeds directly into the settlement gate. Proven/rejected work can become slash/jail candidates; this module does not itself execute consensus slashing.
- Replay key domain: `QRX/POUC/SETTLEMENT-REPLAY/V1`.
- Settlement decision commitment domain: `QRX/POUC/MAINNET-SETTLEMENT/V1`.
- Bounded replay guard rejects duplicate settlement keys in runtime tests. Durable consensus/QRXDB storage of replay keys remains an integration responsibility of the chain state transition.

Security boundaries / non-claims:

- This milestone does not claim that Mainnet PoUC is already switched on in a deployed public network. It provides the activation and settlement gate that must be wired into the live consensus state transition after the readiness predicates are evidenced in deployment.
- `SLASH_CANDIDATE` and `jail_candidate` are deterministic decisions, not unilateral stake destruction. Existing consensus slashing rules remain authoritative.
- Economic readiness is an explicit required attestation; this code does not invent market profitability or reward assumptions.
