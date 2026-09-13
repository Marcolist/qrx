# QRX 0.0.9.3 — AI Model Registry

Implemented a deterministic model registry foundation. Model weights remain in QRX Drive; registry records contain content roots and execution metadata only.

- model ID + version
- architecture + runtime ID
- model manifest / tokenizer / expert-manifest roots
- memory + storage requirements
- LOW / STANDARD / HIGH verification profile
- license metadata identifier
- explicit MoE validation
- SHA3-256 domain-separated deterministic model commitment
- duplicate model/version rejection

Kimi K3 is represented only as a target model record in tests; no performance, licensing, or availability claim is made.
