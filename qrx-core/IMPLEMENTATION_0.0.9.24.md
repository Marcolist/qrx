# QRX 0.0.9.24 — Resource Provider Unified UX

Implemented a single provider-policy foundation and wallet UX for configuring QRX resource sharing across Storage, CPU Compute, AI Accelerator, Model Cache and Network.

## Core

New `qrx_resource_provider` policy/plan layer:
- explicit enable mask per resource class
- user-defined hard caps for storage, model cache, CPU threads and network egress
- accelerator opt-in with independent CUDA and Metal/MLX flags
- host-capability validation: policy cannot reserve more than the host declares available
- opportunity-aware plan: recommendation values are clamped by the user's policy caps
- wallet-approval flag carried into the plan
- no implicit worker start, endpoint publication, transaction signing or QUB spending
- SHA3-256 domain-separated `QRX/RESOURCE-PROVIDER/PLAN/V1` commitment

## GUI Wallet

The existing Resource Globe view now also contains a **QRX Resource Node** panel:
- Storage toggle + GiB cap
- CPU Compute toggle + thread cap
- GPU/AI toggle with CUDA and Metal/MLX choices
- AI Expert / Model Cache toggle + GiB cap
- Network toggle + Mbit/s cap
- mandatory wallet-approval indicator
- local policy summary
- privacy-safe best-visible-region opportunity signal
- optional opportunity preset using only already-public Globe data
- local preferences stored per network + wallet

Saving this panel is deliberately non-activating: it stores local intent only. Provider runtime activation and signed payment/settlement actions remain separate steps.

## Test

`compute_phase149_resource_provider_ux`

Scope boundary: 0.0.9.24 does not claim a completed provider daemon lifecycle, automatic market registration, earnings projection, or autonomous activation.
