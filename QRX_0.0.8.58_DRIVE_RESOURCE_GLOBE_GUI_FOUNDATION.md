# QRX 0.0.8.58 — Drive / QRX-Net / Resource Globe GUI Foundation

Implemented on top of 0.0.8.57 provider discovery.

## Core aggregation API

New `qrx_resource_dashboard` module provides one deterministic snapshot model for Wallet, QRXScan and the future 3D Globe. It combines network capacity, logical user data, shard health, attested provider count, independent ASN count, availability, proof success, provider diversity, storage health, demand factor, active contracts, purchased logical storage and QRX-Net website/domain/cache counters.

Privacy is enforced at the atlas-cell boundary. Region cells below the configured independent-provider threshold remain hidden and are never intended to expose provider IPs, endpoints or exact home-node locations.

## GUI foundation

The wallet now contains first-class `QRX Drive` and `Resource Globe` sections with the stable data contract expected from `resource_dashboard_snapshot`. The UI distinguishes capacity, logical data, redundancy, providers, contracts, demand, region visibility, ASN diversity, domains, hosted sites and cache activity.

The 3D rendering engine itself is deliberately not faked in this phase. The UI consumes privacy-safe regional cells once live daemon RPC wiring is added; until then it explicitly reports that the compiled core schema exists but the live snapshot endpoint is unavailable.

## Validation

New regression: `storage_phase100_resource_dashboard`.
Full suite: 33/33 passed.
