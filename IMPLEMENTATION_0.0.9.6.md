# QRX 0.0.9.6 — Secure Execution Sandbox & Resumable Budget Guard

Implemented:
- deny-by-default sandbox policy for untrusted compute jobs
- wallet keys, host secrets and host filesystem are never grantable
- raw network access denied in V1; future network access must use an allowlisted proxy
- memory, CPU/wall time, output and subprocess ceilings bound to the job descriptor
- executable jobs require explicit SANDBOX_EXEC capability
- budget states: RUNNING, BUDGET_WARNING (>=80%), CHECKPOINTING (>=95%), PAUSED_BUDGET_EXHAUSTED, BUDGET_EXTENDED, RESUMING, COMPLETED
- hard invariant: verified spend can never exceed previously authorized budget
- checkpoint reference + monotonic checkpoint sequence
- explicit user-authorized extension required before resume

This phase defines and tests the portable policy/enforcement contract. Native OS isolation backends (Linux namespaces/seccomp, macOS sandboxing, Windows AppContainer/job-object equivalents) remain separate executor work and are not falsely claimed here.
