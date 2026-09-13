# QRX 0.0.9.5 – Compute Market, Quotes & Escrow

Implemented:

- Versioned compute quote records bound to canonical Job Graph commitments.
- Quote validation against node fee caps and quote expiry.
- Deterministic quote book with duplicate quote rejection.
- Provider ranking uses fixed-point weights for price, normalized compute score, reliability, model locality, estimated latency and verification capability.
- Deterministic provider selection with stable tie-breaks.
- Optional FastTrack capability filtering.
- Pre-execution escrow locks the graph maximum compute budget plus optional FastTrack fee.
- Escrow states: EMPTY, LOCKED, ASSIGNED, SETTLED, REFUNDED, CANCELLED, EXPIRED.
- Settlement rejects actual compute above the user-approved maximum.
- Unused maximum compute budget is refunded deterministically.
- FASTTRACK_DEV_SHARE_BPS = 50 (0.5%) applies only to the FastTrack fee; normal compute execution has no development share in this phase.
- Expired/unexecuted jobs can be refunded without paying providers.

Security / scope notes:

- Quotes are offers, not proof of work. Benchmark attestation and Proof of Useful Compute are later phases.
- No provider can raise a job above the max_fee_atoms committed by the user.
- No surprise-cost path is introduced.
- Model locality and performance claims are scheduling metadata until later verification hardening.

Validation:

- compute_phase131_market_escrow: PASS
- compute phases 126–131 regression: 6/6 PASS
