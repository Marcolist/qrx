# QRX 0.0.7.7 — Generals Phase 6.6

Mainnet release hardening and GUI Wallet production wiring.

- Generals remains bound to the wallet-selected QRX network; sandbox remains isolated.
- Decentralized offline relay now rejects authorization windows above 4096 blocks, deduplicates envelopes and caps active scheduled envelopes per node at 4096.
- Relay still verifies the signed transaction before persistence; relays never receive wallet private keys.
- Autonomous world resolution remains consensus/finality driven.
- GUI Privacy Center now calls the implemented Core shielded commands rather than presenting preview-only controls.
- Shielded address creation is Core-backed; shield, shielded-send and unshield can be executed from the GUI after wallet unlock and confirmation.
- Legacy fake Quantum Swap draft/status/refund placeholder commands were removed. The GUI uses the existing Core HTLC createswap/getswap/listswaps/redeemswap/refundswap path.
- AURA cloud/payment UI no longer pretends a backend exists: cloud/package actions are explicitly disabled until a real production service is configured. Local help remains available.
- Neutrino is not represented as active when it is not bundled; BTC Light continues to use the real BDK/Electrum service.

Release gate: Core must compile and Phase 6.3–6.6 wiring tests must pass. Native Tauri packages must still be built and smoke-tested on each target OS before distributing binaries.
