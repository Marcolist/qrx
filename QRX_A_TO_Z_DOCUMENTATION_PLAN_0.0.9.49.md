# QRX 0.0.9.49 — A–Z Documentation, Setup, Menu Navigation & Operator Handbook

This is the mandatory final documentation phase of the 0.0.9 branch. It is not a short release-note pass. Its acceptance criterion is that a new user can install QRX, create/recover a wallet, understand every normal GUI menu, use QRX Drive/QRX-Net/AURA safely, and diagnose common problems without reading source code.

## Deliverables
1. `QRX_A_TO_Z.md` — canonical complete handbook in the release archive.
2. Offline searchable **Help / QRX A–Z** inside GUI Wallet.
3. Setup quick-start pages for macOS Apple Silicon/Intel, Windows x64/ARM where supported, Linux x64/ARM.
4. CLI/RPC reference checked against the shipped binaries.
5. Provider/operator handbook for Validator, Storage, QRX-Net and AURA Compute.
6. Troubleshooting/error-code index and recovery decision tree.
7. Glossary and architecture diagrams.
8. Versioned migration notes from 0.0.6/0.0.7/0.0.8 to 0.0.9.
9. Printable/exportable HTML/PDF build from the same canonical documentation source; Markdown remains authoritative.

## Required table of contents
### A. What QRX is
QRX Chain/QUB; consensus/finality; validator model; privacy/PQ boundaries; QRX Drive, QRX-Net, AURA, Markets, Generals and Agents; consensus-critical vs local/off-chain state.

### B. Installation and first start
Supported OS/CPU targets; download/signature/hash verification; GUI vs Core/CLI; data directories; Mainnet/Testnet/Alpha/Regtest; firewall/NAT basics.

### C. Wallet setup and recovery
Create/import/recover; passphrase/encrypted keys; recovery phrase/file; backups/restore test; shared multi-network identity; address book and receive QR.

### D. GUI menu-by-menu navigation
Every visible menu gets purpose, normal workflow, safety notes, advanced controls and screenshots/illustrations where useful.

Current menu map:
- Dashboard
- Apps
- QRX Drive
- QRX-Net
- Resource Globe
- Send
- Receive
- Transactions
- Staking
- Validator Mode
- BTC Light
- Quantum Swaps
- Markets
- Agents & Kraken
- Privacy
- Roadmap
- Wallets
- Safety Center
- Address Book
- AURA assistant / AURA Compute automatic provider controls
- Network selector, daemon status and node controls

### E. Payments, balances and transactions
QUB units/fees; send/receive/source control; confirmations/finality; CSV/accounting exports; self-address and wrong-network protections.

### F. Staking, delegation and validators
Staking/delegation; validator lifecycle; slashing/double-signing; safe pause/restart; home-validator catch-up; rewards/tokenomics display.

### G. BTC Light and Quantum Swaps
BTC setup/sync/recovery; shipped sync modes; HTLC/Velocity lifecycle; funding deadlines; refund/redeem; SPV/reorg behavior.

### H. Privacy
Stealth/rotation/shielded QUB; shield/transfer/unshield; hidden balances/proof boundaries; remaining observable metadata.

### I. QRX Drive / Proof of Storage
Upload/download; shards/redundancy; provider discovery; multi-provider fetch/resume/repair; contracts/PoSTOR; capacity/file-size policy.

### J. QRX-Net and Browser
.qrx domains; hosting/site packages; WWW + QRX browser; DApp consent bridge; ad/reward/youth-safety controls.

### K. Resource Globe / provider mode
Privacy-safe regional cells; Storage/Compute/AI/Model Cache/Network limits; opportunity/hosting missions; wallet-approval boundary.

### L. AURA for beginners
**Automatic is the default**; hardware/runtime auto-selection; why users normally do not choose GGUF/MLX/CUDA; signed model catalog; cache budget and Eco/Balanced/Performance; model/expert discovery; QRX Drive replication; hot-expert migration; NAT relay; durable retries; privacy/security limits.

### M. AURA advanced/operator reference
Model Registry/manifests/content assets; availability gossip; autoplacement; MoE pods/expert locality/prefetch; runtime adapters/package signatures; relay failover/supervisor; journals; benchmark/telemetry interpretation.

### N. Markets, assets and agents
Native assets/regulatory flags; order book; Kraken agent manager/security boundaries; paper/live distinctions.

### O. CLI/RPC
Command groups/examples; JSON-RPC methods; network/wallet selection; safe scripting.

### P. Backups, upgrades and migrations
Compatible wallet locations; pre-upgrade backup; mandatory upgrades; rollback boundaries; QRXDB/WAL recovery tools.

### Q. Security model
Key storage; hybrid PQ; signed model/provider announcements; untrusted-relay model; software/model verification; least privilege.

### R. Troubleshooting
Daemon startup; locked/imported wallets; peers/sync; provider visibility; model placement/download; CUDA/MLX/runtime; relay/NAT; degraded Drive objects; BTC sync; logs/recovery paths.

### S. Developer/integrator appendix
Source tree; build-all-targets; CTest; adding model catalog entries/runtime adapters; API/ABI boundaries; release manifest/signing.

### T. Glossary + FAQ
Plain-language QRX/AURA terminology and beginner FAQs.

## Documentation quality gates
- every GUI navigation item documented,
- every user-facing setting has a description and safe default,
- commands/examples checked against final 0.0.9 binaries,
- no obsolete Alpha-only screenshots in Mainnet instructions,
- destructive/recovery actions explicitly marked,
- beginner route first; advanced route separate/collapsible,
- English canonical text plus translation-key integration for in-wallet Help,
- documentation version/commit/release hash in footer.
