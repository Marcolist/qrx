# QRX 0.0.7.1 Privacy Layer – Phase 1

## Scope

Phase 1 adds wallet-level privacy without changing QRX consensus privacy primitives or pretending that the current shielded skeleton is production ZK privacy.

### Recoverable Address Rotation

- GUI Receive has `Use a fresh address for every payment`, enabled by default per wallet.
- Opening a new QUB payment request creates a fresh hybrid Ed25519 + ML-DSA65 receive address.
- Additional addresses require an existing `recovery.qrxseed`.
- The canonical recovery seed now carries an encrypted `qrx-address-recovery-v1` extension containing all rotated receive keypairs.
- The extension encryption key is domain-separated and derived from the canonical Ed25519 private key; the recovery phrase is not stored on disk.
- Recovery with the same recovery phrase + `recovery.qrxseed` restores the primary identity and all rotated receive addresses.
- If the recovery extension cannot be updated, the new address is removed from the active address index and is not announced as usable.

### QUB Source Control

QUB is account/address based, so Phase 1 deliberately implements Source Control rather than Bitcoin-style UTXO coin control.

The Send view offers:

- `Automatic` – legacy-compatible canonical-wallet sending.
- `Privacy optimized` – selects a funded wallet-controlled address, preferring non-primary, non-reused and lower-history addresses.
- `Select address` – explicit source address selection.

Core/daemon support:

- backend: `qrx send-from <wallet-dir> <chain-dir> <source-address> <to> <amount> <memo> [node-dir]`
- RPC/CLI: `sendfromaddress <source> <recipient> <amount> [memo]`
- signing verifies that the selected source has a corresponding wallet key directory and that its Ed25519 public key derives the selected address.

### Privacy Analysis

The GUI analyzes wallet-controlled addresses and reports:

- address reuse signal from history count,
- primary-vs-rotated source selection,
- source balance,
- a simple wallet privacy score,
- explicit warning when a self-transfer links two wallet-controlled addresses on-chain.

This score is advisory, not an anonymity guarantee.

## Safety boundaries

- Existing shielded/stealth code is unchanged by this phase.
- No hidden balances or KYC credential layer is enabled here.
- No claim of anonymity or untraceability is made.
- Automatic address rotation is blocked for wallets without `recovery.qrxseed`, preventing creation of GUI-managed receive keys that cannot be restored from the documented recovery pair.

## Validation performed

- Core CMake configure/build with OpenSSL 3.5.6: PASS.
- `qrx`, `qrxd`, `qrx-cli` and QRXDB tools linked successfully: PASS.
- Fresh wallet -> rotated address -> recovery extension created: PASS.
- Recovery using original phrase + updated `recovery.qrxseed`: primary and rotated address list identical: PASS.
- Wallet without `recovery.qrxseed` refuses rotated-address generation with a recovery-first message: PASS.
- Source-address signing path reached transaction validation using the selected child keyset: PASS (application then failed as expected in an unfunded test chain).
- GUI JavaScript syntax (`node --check`): PASS.
- GUI/Core compatibility audit: PASS, 29 command names.
- Rust/Tauri compile was not run in this environment because Rust/Cargo is not installed; compile on target macOS/Windows/Linux remains required.
