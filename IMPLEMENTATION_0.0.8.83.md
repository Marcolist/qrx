# QRX 0.0.8.83 — Final adversarial / release-readiness implementation gate

## Result
QRX 0.0.8 feature roadmap: **DONE**.

This checkpoint closes the implementation roadmap, not an external Mainnet certification.

## Gate performed
- Full RelWithDebInfo build with `QRX_BUILD_TESTS=ON`.
- Fixed NDEBUG/assert harness gap affecting newer assert-driven integration tests in release-like configurations.
- Added `net_phase125_final_readiness` consolidated invariant regression.
- CTest result: **58/58 PASS**.

## Consolidated invariants
- QRX sponsored-ad reward split totals exactly 10,000 bps and requires at least three provider receipts.
- Child and Teen policy defaults fail closed for Sponsored Ads, Viewer Rewards and unrated content.
- `qrx://` sandbox never allows DNS, wallet IPC, filesystem or external network and rejects traversal.
- Hosting Mission incentives remain capped at 1.20x.

## Remaining Mainnet launch gates outside the 0.0.8 feature roadmap
1. Real multi-host testnet chaos/soak with provider churn and network partitions.
2. Windows/macOS/Linux/ARM GUI/Tauri builds and native packaging/signing.
3. Production-like 0.0.7 -> 0.0.8 migration rehearsal and rollback.
4. Independent security review plus structured fuzzing/sanitizer campaigns.
5. Release reproducibility, artifact signing and incident/rollback drills.

## Next branch
0.0.9 — QRX Compute.
