# QRX 0.0.9.26 – Compute Adversarial Testnet

Status: DONE (2026-09-09)

This phase adds a deterministic adversarial-evidence evaluator above the existing PoUC receipt, optimistic verification, scheduler, MoE and Resource Provider layers.

Implemented attack classes:
- fake/overstated compute
- result commitment mismatch
- model/runtime mismatch
- verifier collusion concentration
- Sybil provider sets
- model-cache fraud
- slow workers
- queue starvation
- WAN partitions
- pod failures

Implemented response primitives:
- force challenge
- reject result
- freeze payout
- quarantine provider/evidence set
- mark as slash candidate
- reroute
- drain provider
- redundant retry

Important safety boundary:
- 0.0.9.26 does NOT directly slash stake or mutate Mainnet consensus.
- `SLASH_CANDIDATE` is evidence output for later consensus/governance enforcement.
- `consensus_slash_applied` is deliberately always 0 in this phase.
- no IP addresses, exact coordinates, host paths or wallet secrets are included in adversarial evidence.

Evidence is committed with SHA3-256 domain separation:
`QRX/COMPUTE/ADVERSARIAL-EVIDENCE/V1`

The evaluator is deterministic, integer-only for risk calculations, and emits bounded risk/challenge values in basis points.

Test coverage:
- fake compute deficit detection
- deterministic evidence commitments
- mismatched result rejection/freeze
- model/runtime mismatch rejection
- colluding verifier concentration
- Sybil provider-set detection
- cache-hit fraud
- slow worker draining/reroute
- queue starvation
- WAN partition
- pod failure
- healthy near-match false-positive guard

CTest target: `compute_phase151_adversarial_testnet`
Full registered CTest suite: 84/84 PASS on Linux Release build with `QRX_REQUIRE_PQC=OFF` developer build mode.

Scope not claimed yet:
- live chaos-injection network harness
- production distributed verifier assignment
- consensus slashing execution
- automatic stake jailing
- real multi-region WAN/pod outage test fleet
- Mainnet PoUC activation
