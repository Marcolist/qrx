# QRX App Platform — 0.0.9 Foundation / 0.0.10 Ecosystem

## 0.0.9 Genesis foundation

0.0.9 deliberately ships only the local platform contract:

- `.qrxapp` ZIP format v1
- wallet App Registry
- isolated QRX App Host (`sandbox=allow-scripts`, no same-origin)
- permission allowlist and per-call Rust enforcement
- QRX Mini JS SDK v1
- local `.qrxapp` installation/uninstallation
- Developer Mode for live unpacked folders
- `Hello QRX` demo source/package
- no private-key exposure and no app-side transaction signing

Sideloaded apps are clearly marked unverified in 0.0.9. QRX does not infer trust merely because a package contains a signature-looking file.

## 0.0.10 ecosystem

```text
                   QRX APP PLATFORM

Developer
   │
   ├── Mini SDK
   │
   └── .qrxapp
          │
          ▼
   Developer Signature
          │
          ▼
       QRX Drive
          │
          ▼
       QRX-Net
          │
          ▼
   QRX App Directory
          │
          ▼
       QRX Wallet
          │
   ┌──────┴────────┐
   ▼               ▼
Local Compute    QRX Compute
```

0.0.10 adds package-signature verification, developer identity/trust, immutable package publication through QRX Drive, QRX-Net discovery, the QRX App Directory, update metadata and explicit compute capabilities. The 0.0.9 manifest/sdk contract is the compatibility base so apps do not need to be rewritten when the distribution layer arrives.
