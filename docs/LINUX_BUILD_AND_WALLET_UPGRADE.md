# QRX 0.0.9.61 — Linux build and existing-wallet upgrade

## The two secrets are different

**Wallet passphrase** unlocks the encrypted private-key PEM files. It is not the recovery phrase.

**Recovery phrase + matching `recovery.qrxseed`** are the recovery material. Never pass recovery words to `qrxd --wallet-passphrase`, `--wallet-passphrase-file`, `--wallet-passphrase-stdin`, `QRX_PASSPHRASE`, or `qrx-cli walletpassphrasehex*`.

## 1. Install build prerequisites

The full QRX release includes Rust/Tauri components. If the release script reports `Missing build dependency: cargo`, install Rust using your distribution's supported Rust package or rustup, then verify:

```sh
rustc --version
cargo --version
```

Also install the C/C++ build toolchain and the Linux libraries required by Tauri/WebKit/Vulkan for your distribution. Run the QRX release plan again after dependencies are present; do not delete an existing wallet to fix a compiler dependency.

## 2. Back up before upgrading

Stop `qrxd` first. Find the wallet selected by your `--datadir`/`--wallet` configuration and make an offline copy of the complete wallet directory. Preserve file permissions. At minimum, verify the backup contains `address.txt`, the private/public key PEM files, and `recovery.qrxseed` when that wallet has recovery enabled.

Never initialize a new wallet on top of the old wallet directory. Never replace old keys with files from another wallet.

## 3. Build/install the new release

Build the target matching the host (`linux-x64` or `linux-arm64`). A missing Cargo message is a build-host prerequisite failure, not a wallet migration request.

## 4. Re-open the existing wallet

Start the new `qrxd` with the same network/datadir/wallet selection used by the existing installation. QRX opens an existing wallet in place; it must not silently overwrite its keys. Check the address after startup against the backed-up `address.txt` before sending funds or enabling validator signing.

## 5. Unlock safely

Preferred unattended method:

```sh
chmod 600 /secure/path/qrx.pass
./qrxd --network alpha --wallet node1 --wallet-passphrase-file /secure/path/qrx.pass
```

Pipe/secret-manager method:

```sh
printf '%s\n' "$QRX_SECRET" | ./qrxd --network alpha --wallet node1 --wallet-passphrase-stdin
```

`--wallet-passphrase PASS` remains compatibility-only because command-line arguments may be observable by other tooling. `QRX_PASSPHRASE` remains available for supervised deployments but must be protected as a secret.

For an already running daemon, the GUI uses the local wallet session RPC. `walletpassphrasehexfor <wallet> <hex>` accepts the **wallet passphrase encoded as hex**, not recovery words. Lock it again with `walletlockfor <wallet>`.

## 6. Recovery is a separate workflow

Do recovery only if the existing wallet cannot be opened/restored from its intact backup. The Core frontend exposes:

```text
qrx wallet-recover <wallet-dir> <recovery-file>
```

Use the matching `recovery.qrxseed` and the recovery phrase requested by the recovery workflow. Restore into a new/empty destination, verify the restored address against the original `address.txt`, and keep the old backup untouched until verification is complete.

## Safety checklist

- Stop daemon before copying wallet files.
- Keep at least one offline backup before first launch with a new release.
- Wallet passphrase != recovery phrase.
- Recovery phrase + matching `recovery.qrxseed` are recovery material.
- Never paste recovery words into daemon startup flags.
- Verify the wallet address before transactions or validator signing.
- Do not use a missing compiler/build dependency as a reason to recreate a wallet.
