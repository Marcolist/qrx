# QRX 0.0.9.20 — Resource Globe Expansion

Implemented a scheduler-facing, privacy-safe regional resource aggregation layer that extends the 0.0.8 storage atlas concept to Storage, Compute, AI, Model Cache, Network and Opportunity.

## Implemented
- coarse region/cell aggregation; public ABI contains no coordinates, IPs, hostnames or addresses
- unique-provider privacy threshold, default 3; sparse cells remain non-public
- STORAGE / COMPUTE / AI / MODEL CACHE / NETWORK / OPPORTUNITY layer mask
- aggregate storage free bytes, compute NCU, AI milli-tok/s, model-cache bytes and egress Mbps
- aggregate latency, utilization, reliability and demand
- deterministic capacity, latency, demand and opportunity classes
- layer-specific and composite opportunity scores
- scheduler-facing summaries rather than visualization-only metadata
- SHA3-256 `QRX/RESOURCE-GLOBE/CELL/V1` commitment
- duplicate observations from one provider cannot inflate the public privacy threshold

## Non-claims
This phase does not claim live geolocation, a finished GUI globe, or production telemetry ingestion. Exact provider/home coordinates are deliberately absent from the public aggregation ABI. 0.0.9.21 will consume these summaries for concrete configuration recommendations/opportunity decisions.

## Test
`compute_phase145_resource_globe`
