# QRX 0.0.9.46 — Model Catalog Governance, Provenance, License Policy, Update & Rollback

Implemented:
- `qrx_aura_model_governance.{h,c}`.
- signed governance decisions keyed by model ID/version with monotonic sequence.
- APPROVE, BLOCK and ROLLBACK actions.
- exact model provenance binding (manifest root, publisher, license, upstream source/commit metadata).
- explicit license allow/deny policy; governance cannot override a deny rule.
- governed catalog-ingest wrapper preserving the existing signed catalog path.
- controlled update and signed rollback to a previous manifest root.
- phase171 adversarial checks for provenance mismatch and unauthorized governor.

Validation: phase171 PASS; later full suite PASS.
