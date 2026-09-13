# QRX 0.0.9.0 — Compute Foundation

## Scope
First code checkpoint for the 0.0.9 Compute branch. This implements the shared heterogeneous compute descriptor layer without enabling arbitrary remote code execution or Mainnet compute consensus.

## Implemented
- `QrxComputeCapabilities`: Linux/Windows/macOS and x86-64/ARM64 scheduling metadata.
- CPU/GPU/NPU/AI accelerator descriptors without hardware-identity or endpoint fields.
- `QrxComputeTaskDescriptor`: deterministic runtime/workload/input commitments, resource bounds and fee ceiling.
- Foundation sandbox invariant: Compute V1 descriptors requesting host commands, filesystem writes or external network access are rejected.
- Provider eligibility checks for architecture, memory and sandbox availability.
- Coarse privacy-safe scheduling fingerprint that intentionally excludes serial numbers, hostnames, IP addresses and exact free-memory values.
- `compute_phase126_foundation` regression test.

## Deliberately NOT implemented yet
- arbitrary workload execution
- Mainnet compute activation
- PoUC settlement/rewards
- escrow/FastTrack
- model registry
- WAN MoE / Kimi K3
- autonomous AURA/Ouroboros behavior

These remain later 0.0.9/0.0.10 roadmap phases.
