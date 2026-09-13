# QRX Chain 0.0.9

**The decentralized computer for value, storage, networking, AI and applications.**

QRX is a from-scratch blockchain/network stack centered on **QUB (QUBITCOIN)**. Version 0.0.9 brings the Chain, GUI Wallet, QRX Drive, QRX-Net, AURA, native markets, privacy foundations and the QRX application layer into one platform.

> **Release status:** Genesis-era 0.0.9 source. Treat Mainnet and advanced privacy/compute features conservatively. Passing regression tests is not a substitute for an independent security audit.

## The QRX stack

- **QRX Chain / QUB** — BFT finality, staking, delegation, slashing, governance, native assets and tokenomics.
- **VELOCITY** — parallel/MVCC execution, deterministic settlement, native markets and cross-chain foundations.
- **QRX Drive** — encrypted decentralized storage, erasure coding, multi-provider retrieval, resume and repair.
- **QRX-Net** — `.qrx` naming, decentralized site publishing, service discovery and the integrated secure browser foundation.
- **AURA / Proof of Useful Compute** — local-first AI, distributed compute, signed runtimes/models, heterogeneous provider hardware and MoE scheduling.
- **Privacy** — address rotation, stealth receiving, shielded QUB and hidden-balance/proof foundations with fail-closed activation boundaries.
- **BTC Light + Quantum Swaps** — Bitcoin light-wallet/SPV integration and HTLC/VELOCITY cross-chain settlement.
- **Markets & Agents** — QRX-native order book, trading automation, paper trading and explicitly authorized external gateway paths.
- **QRX Apps** — sandboxed `.qrxapp` packages, App Registry, permissions and Mini JavaScript SDK.
- **QRX Generals** — QRX-native persistent strategy game/application.
- **QRX Upscaler** — local image/batch/video enhancement foundation, prepared for future distributed QRX Compute workloads.

## Post-quantum readiness

QRX is designed for **quantum resilience and cryptographic migration**, not around the claim that every dependency is already “quantum proof”. The wallet architecture includes hybrid **Ed25519 + ML-DSA-65** identity/signature work, SHA-3/content-addressed verification and explicit cryptographic trust boundaries.

## QUB economy

Protocol emission is separated from paid network services.

- Initial block subsidy: **0.25 QUB**
- Target block time: **~10 seconds**
- Halving interval: **12,614,400 blocks (~4 years)**
- Hard supply ceiling: **21,000,000 QUB**
- Current subsidy schedule is below that ceiling; 21M is a maximum, not the issuance target of the current curve.

Service rewards are funded by users/advertisers rather than creating a second inflation stream:

| Layer | Distribution |
|---|---|
| QRX Drive | 97.5% provider · 2% resilience/repair · 0.5% development |
| Compute FastTrack | 90% provider · 9.5% network · 0.5% development |
| Advertising | 55% delivery · 25% publisher · 15% viewer · 4.5% network · 0.5% development |

## AURA — local first, network when useful

AURA is the user-facing AI/useful-compute layer. Automatic mode discovers the host hardware, selects a compatible signed runtime, verifies it, selects compatible model/expert assets and can use QRX provider/Drive infrastructure when available.

The runtime architecture covers x86-64 and ARM64 CPU paths, Apple Silicon/Metal and NVIDIA CUDA paths including Pascal/P40-class capability representation. Model bootstrap/catalog work includes Qwen, DeepSeek and Kimi families with governed provenance/license handling and content-addressed assets.

## GUI Wallet

The Tauri GUI and native Core share the QRX wallet/node architecture. Current 0.0.9 work includes:

- Windows, Linux and macOS release targets; x64/ARM64 where supported
- shared wallet management, Recovery Center and Safety Center
- QUB + BTC Light workflows
- QRX Drive, QRX-Net, AURA, Markets, Privacy and Apps views
- integrated secure browser foundation
- responsive full-width application workspaces
- **55 locale catalogs** with complete current wallet key parity

## Apps & Mini JS SDK

0.0.9 establishes the local application foundation:

- `.qrxapp` v1 package format
- sandboxed App Host
- explicit permission model
- local App Registry and Developer Mode
- Mini JS SDK
- controlled identity/balance/network reads
- app-scoped storage
- wallet-mediated payment requests without exposing private keys

Distributed publishing, signatures, QRX Drive/QRX-Net distribution and the broader app directory are part of the 0.0.10 direction.

## Staged Mainnet protocols

Resource protocols are **fail-closed** and are not activated merely because a date arrives. Activation requires readiness/soak criteria plus threshold-signed on-chain governance scheduling.

| Protocol | Operational target |
|---|---:|
| `DRIVE_V1` | 2026-11-30 17:00 UTC |
| `QRX_NET_V1` | 2026-12-07 17:00 UTC |
| `ADVERTISING_V1` | 2026-12-15 17:00 UTC |
| `COMPUTE_POUC_V1` | 2027-01-31 17:00 UTC |

## Build

Core development build:

```bash
cd qrx-core
cmake -S . -B build
cmake --build build -j
```

Run the registered native regression suite:

```bash
cmake -S . -B build-tests -DQRX_BUILD_TESTS=ON
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
```

Unified host build:

```bash
bash scripts/build-all-targets.sh --target host
```

Supported native entry points are `qrx`, `qrxd` and `qrx-cli`.

## Documentation

Start with **`docs/QRX_A_TO_Z_0.0.9.md`**. It is the canonical offline A–Z handbook for the 0.0.9 platform and covers installation, wallet/recovery, staking, Drive, QRX-Net, AURA, privacy, BTC Light, Quantum Swaps, markets, agents, governance, CLI/RPC, Apps/SDK and troubleshooting.

## 0.0.10 — Ouroboros

0.0.10 expands the 0.0.9 local application foundation toward the distributed QRX application ecosystem: developer signatures, QRX Drive publishing, QRX-Net distribution, app discovery/update/reputation, local/QRX Compute APIs and broader decentralized governance.

The longer-term architecture connects **QRX Chain + QRX Drive + QRX-Net + AURA + QRX Compute + QRX Apps**. QRX Chain is evolving toward the decentralized computer; **QRX OS (`qrxos.com`)** is planned as its operating-system layer.

## Community & links

- Website: https://qrxchain.org
- Explorer: https://qrxscan.com
- GitHub: https://github.com/phoenixkonsole/qrx
- Discord: https://discord.gg/4G2Vyj9jYf
- Bitcointalk: https://bitcointalk.org/index.php?topic=5580957.0
- QRX OS: https://qrxos.com

## Security notice

QRX 0.0.9 contains substantial internal hardening and regression coverage, but this repository must not present internal tests as an independent external audit. Keep backups, verify release hashes, test recovery, use advanced/privacy/provider functionality cautiously, and follow the release documentation for feature-gated Mainnet functionality.

---

**QRX — value, verification and digital sovereignty for the AI age.**
