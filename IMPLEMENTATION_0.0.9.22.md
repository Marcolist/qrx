# QRX 0.0.9.22 — Animated AI Task Globe

Implemented the privacy-safe task-flow data model that can drive an animated Resource Globe in Wallet/AURA without revealing sensitive node locations.

## Implemented
- Deterministic task event stream: Submitted → Scheduled → Compute → Expert Groups → Result/Failed.
- Aggregated public animation frames with provider, region, pod and expert-group counts.
- FastTrack state and deterministic progress in basis points.
- Privacy threshold: aggregate provider/region/pod/expert counts remain suppressed until the configured minimum distinct-provider threshold is reached (default 3).
- Internal provider IDs and pod IDs are used only for deduplication; they are never copied into public frames.
- Strict single-task timeline, monotonically increasing sequence numbers, and no events after terminal Result/Failed.
- SHA3-256 domain-separated frame commitment: `QRX/TASK-GLOBE/FRAME/V1`.

## Scope boundary
This phase implements the Core protocol/data-feed foundation for animation. It does not claim a production graphical globe renderer, live P2P telemetry transport, or disclosure of exact provider locations. The GUI can consume these frames in a subsequent wallet wiring phase.
