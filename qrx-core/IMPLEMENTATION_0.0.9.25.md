# QRX 0.0.9.25 – Provider Runtime Lifecycle & Market Registration

Date: 2026-09-09

## Implemented

This phase connects the 0.0.9.24 QRX Resource Node policy/plan layer to a deterministic provider runtime lifecycle and a wallet-gated market registration descriptor.

Runtime states:

- CONFIGURED
- READY
- REGISTERED
- SERVING
- DRAINING
- OFFLINE

A provider cannot move from READY into REGISTERED when the active plan requires wallet approval unless a valid approval record matches all of:

- provider identity
- QRX network
- exact plan commitment
- current chain-height validity window

The approval record is an input from the wallet/signing layer. This phase does not fabricate signatures and does not claim end-to-end wallet RPC wiring.

## Safe capacity changes

The runtime tracks active jobs and reserved storage, model-cache, CPU-thread and network capacity. A new plan is applied immediately only when it cannot invalidate currently reserved work.

If a requested shrink would cut below live reservations, or disable a resource class that an active job may depend on, it becomes a pending plan and the provider enters DRAINING. Existing work can finish; new serving reservations are blocked. Once reservations are released, the pending plan becomes active and returns to READY so that the changed capacity must be explicitly re-registered before serving again.

Shutdown is rejected while jobs or resource reservations remain.

## Market registration

A market registration exposes the approved plan's bounded public capacity metadata and increments a deterministic registration revision. It does not start a worker by itself.

Registration commitment domain:

`QRX/RESOURCE-PROVIDER/MARKET-REG/V1`

The registration binds:

- provider ID
- network
- active plan commitment
- revision
- resource mask and capacity caps
- accelerator/CUDA/Metal-MLX flags
- wallet-approval state

## Build/test hardening

Also fixed the Phase 148 CTest registration so `compute_phase149_resource_provider_ux` is no longer passed accidentally as an argv parameter to the Phase 148 executable.

New test:

`compute_phase150_provider_runtime_market`

Coverage includes wallet-approval gating, market revisioning, deterministic commitments, serving reservations, deferred shrink/draining, release/apply, re-registration, safe drain/offline shutdown and network-mismatch rejection.

Full registered suite after implementation: 83/83 PASS on the Linux build environment.

## Scope boundary

Not claimed in 0.0.9.25:

- production daemon worker launch
- live P2P market announcement transport
- GUI-to-daemon activation RPC
- real wallet signature verification transport
- live reward settlement
- production Mainnet provider activation

Those require later end-to-end/runtime and adversarial phases.
