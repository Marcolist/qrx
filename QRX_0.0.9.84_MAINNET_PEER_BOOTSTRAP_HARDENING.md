# QRX 0.0.9.84 — Mainnet Peer Bootstrap Hardening

- Adds the three deployed Mainnet peers as built-in bootstrap fallbacks:
  - `node01.qrx.mey-solution.de:26660`
  - `89.143.197.28:26660`
  - `174.165.210.227:26660`
- Retains `seed1.qrxchain.org`, `seed2.qrxchain.org`, and `seed3.qrxchain.org` for DNS-based bootstrap continuity.
- Expands the low-level default seed list from 3 to 6 entries.
- Hardens `getpeerinfo` JSON so backend section markers (`[peers]`, `[known]`) and `no peers` are not exposed as peer endpoints.
- Existing node directories receive newly configured profile seeds through the existing append-unique startup path.
