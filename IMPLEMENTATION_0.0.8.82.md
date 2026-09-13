# QRX 0.0.8.82 — QRX-Net Globe + Hosting Missions

This step completes the privacy-preserving Resource UX layer for QRX-Net hosting.

## Hosting mission policy

Missions are derived only from atlas cells that already satisfy the public privacy threshold (currently at least 3 providers). Sparse/hidden regions are removed before mission calculation so a mission response cannot reveal that a single home node or provider exists in a location.

A mission contains only an aggregated region label, additional free proven capacity wanted, additional provider count wanted, opportunity score and a bounded incentive multiplier. It never contains provider IDs, IP addresses, endpoints, precise coordinates or viewer/browser history.

The default mission target is 10 providers and 1 GiB free proven capacity per visible cell. The multiplier is 1.00x–1.20x based on the existing atlas opportunity score. This is an incentive signal only; placement and provider selection remain decentralized/consensus-driven.

## Interfaces

- daemon/CLI: `gethostingmissions`
- Tauri: `resource_hosting_missions`
- Wallet Resource Globe: Hosting Missions panel with live refresh

## Verification

`net_phase124_hosting_missions` verifies hidden-cell suppression, demand calculation and the 1.20x cap.

Full Core CTest: **57/57 PASS**.
