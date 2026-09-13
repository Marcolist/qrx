# QRX 0.0.9.47 — Heterogeneous WAN Campaign, Quality/Cost/Energy Routing & Production Telemetry

Implemented:
- `qrx_aura_production_routing.{h,c}`.
- heterogeneous production offers across CPU/MLX/CUDA/other backends and regions.
- Eco/Balanced/Performance scoring across task quality, observed quality, latency, price, energy, reliability and locality.
- hard route constraints for quality, latency, price, energy, reliability and context.
- cumulative production telemetry for requests/tokens/quality/latency/cost/energy/WAN/failures.
- deterministic route, telemetry and campaign commitments.
- reproducible heterogeneous WAN campaign runner.

Validation: phase172 PASS; later full suite PASS.
