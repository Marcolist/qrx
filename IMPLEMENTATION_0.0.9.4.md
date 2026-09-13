# QRX 0.0.9.4 – Compute Job Protocol & Job Graphs

Implemented 2026-09-09.

## Scope

- One bounded, deterministic job-graph format for AURA, Wallet, dApps and external API clients.
- Job types: AI inference, code generation/execution, document generation, data analysis, image generation, compile, test, conversion, research and custom sandboxed work.
- Opaque QRX Drive/content references for job inputs and artifacts; large payloads remain outside the graph.
- Per-node runtime, model identity, memory/runtime/output/fee limits and narrow capability masks.
- Execution/build/test/custom jobs require explicit sandbox execution capability.
- AI-oriented jobs bind to a model ID/version and require model-inference capability.
- Directed-acyclic graph validation with missing-edge, duplicate-node, duplicate-edge and cycle rejection.
- Deterministic topological scheduling with stable node-ID tie-breaking.
- Graph-level fee ceiling with overflow protection.
- SHA3-256 domain-separated deterministic graph commitment (`QRX-COMPUTE-JOB-GRAPH-V1`).
- Canonical commitments are independent of node/edge insertion order.

## Security invariants

The Job Protocol describes work; it does not grant arbitrary host privileges. Host command, filesystem and network policy remains the responsibility of the QRX Compute sandbox/runtime. Capability bits are allowlisted and unknown privileges are rejected.

## Tests

New test: `compute_phase130_job_protocol`

Verified locally:
- valid AI -> compile -> test graph accepted
- deterministic topological order
- canonical graph commitment independent of insertion order
- cyclic graph rejected
- total-fee cap bypass rejected
- unsandboxed execution job rejected

Focused Compute regression verified: phases 126, 127, 128, 129 and 130 all PASS.
The incoming 0.0.9.3 snapshot documented 62/62 full-suite PASS. A full all-target rebuild was started for 0.0.9.4 but exceeded the execution window; therefore this checkpoint does not claim a new full-suite 63/63 run.

## Next

0.0.9.5 – Compute Market, Quotes & Escrow.
