# QRX 0.0.9.8 — AURA Agent Runtime & Tool API

Implemented the first bounded AURA orchestration runtime on top of the existing QRX Compute Job Protocol. AURA remains a client/orchestrator and does not receive a privileged consensus or host-execution path.

## Implemented

- Versioned `QrxAuraAgentManifest` with agent/model identity, capability mask, maximum steps, per-job fee ceiling and per-session fee ceiling.
- Domain-separated SHA3-256 manifest commitments.
- Bounded AURA sessions with owner binding, authorized spend, verified spend, step count and OPEN / BUDGET_WARNING / PAUSED / COMPLETED / CANCELLED states.
- Versioned tool calls with deterministic commitments.
- Tool capability checks for inference, artifact access, job operations, streaming, research and code workflows.
- Explicit user-approval gate for code execution / compile / test when required by the agent manifest.
- Conversion of eligible AURA tool calls into ordinary `QrxComputeJobNode` records. Generated code execution, compile and test jobs receive only the existing sandbox capability and remain subject to 0.0.9.6 policy enforcement.
- AURA cannot grant wallet-key visibility, host-secret visibility, host filesystem access or unrestricted networking.
- Session spend cannot exceed prior user authorization. A call is rejected if its requested budget exceeds either the per-job ceiling or the session's remaining authorization.

## Tool boundary

Metadata/local-client tools such as status/read/write orchestration are validated by the AURA runtime but are not automatically converted into provider execution jobs. Provider execution is created only for inference, research and code/build/test workloads that map to the shared Compute Job Protocol.

## Verification

`compute_phase134_aura_runtime`: PASS.
Focused Compute regression phases 126–134: 9/9 PASS.

This is not yet AURA Chat UI. Conversation persistence, token streaming UX, project context and wallet integration are the next phase, 0.0.9.9.
