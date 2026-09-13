# QRX 0.0.9.12 – AURA Code Generation, Build & Test Workspace

Implemented:

- immutable project-snapshot binding through the existing AURA Project commitment
- versioned AURA code workspace manifest
- staged code workflow: GENERATE, EXECUTE, COMPILE, TEST
- explicit per-stage input/output references, runtime, fee, runtime and output limits
- hard aggregate workspace budget
- execution/compile/test stages require explicit user approval
- code-generation stage can be prepared without granting host execution rights
- deterministic SHA3-256 domain-separated workspace commitments
- conversion of approved workspace stages into ordinary AURA Tool Calls
- compile/test execution therefore continues through the existing sandbox, budget guard and Compute Job Protocol
- project/reference owner binding
- protection against fee-budget overflow / underflow

Security invariants:

- AURA does not write arbitrary host files.
- AURA cannot execute/compile/test merely because it generated code.
- Compile/test stages carry explicit approval and still require the Agent Manifest policy to allow execution.
- Workspace stages cannot exceed the pre-authorized aggregate budget.
- Source project and outputs are referenced through QRX Drive / artifact refs rather than embedded into protocol metadata.
- No wallet seed/private key access is introduced.

Focused validation:

- compute_phase134_aura_runtime: PASS
- compute_phase135_aura_chat: PASS
- compute_phase136_aura_artifacts: PASS
- compute_phase137_aura_code_workspace: PASS

A fresh complete historical test-suite run is not claimed by this milestone.
