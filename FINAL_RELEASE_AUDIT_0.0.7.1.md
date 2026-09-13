# QRX 0.0.7.1 — Final Release Audit + Cross-Platform Build/Test Gate

Date: 2026-09-04

## Result

**Native host final audit: PASS** on Linux x86-64.

The release pipeline now targets five native platforms in parallel:

| Target | Release plan / CI gate | Native build executed in this environment | Status |
|---|---:|---:|---|
| Linux x86-64 | PASS | Core + CLI + qrxd + QRXDB tools + CTests | PASS |
| Linux ARM64 | PASS | No — requires ARM64 native runner | READY FOR MATRIX |
| macOS x86-64 | PASS | No — requires Intel macOS runner | READY FOR MATRIX |
| macOS ARM64 | PASS | No — requires Apple Silicon runner | READY FOR MATRIX |
| Windows x86-64 | PASS | No — requires Windows/MSVC runner | READY FOR MATRIX |

The GitHub Actions matrix uses native runners for all five targets and performs the common regression/security gates before building each complete target release. This environment does not contain Rust/Cargo or non-Linux native runners, so it would be incorrect to claim that the four non-host installers were compiled here.

## Gates passed locally

- Five-target build plans validate and preserve dependency order.
- GUI/Core compatibility: **29 GUI Core commands validated**.
- RPC/network security: localhost-only by default; non-loopback requires explicit opt-in + credentials.
- 0.0.6 regression surface: **all 59 legacy CLI commands present**, dashboard RPC and mobile raw-TX pipeline retained.
- Python release/tool/wallet tests: **53/53 PASS**.
- Front-end JavaScript syntax: **PASS**.
- Clean Core Release build: **PASS**.
- VELOCITY CTests: **6/6 PASS**.
- Privacy 0.0.7.1 release surface: recoverable receive rotation, Source Control and privacy analysis present.
- Package/CI naming updated to **0.0.7.1**.

## Audit maintenance fixes

The final audit uncovered three stale test/audit assumptions left from earlier architectural stages. They were corrected rather than suppressing failures:

1. BTC Light service test still expected `src-tauri/src/bin/qrx-btc-wallet-service.rs`, although the service was intentionally moved to `GUIWALLET/btc-wallet-service/src/main.rs` so Tauri cannot mistake it for the GUI main executable.
2. Phase 4B/4C audits expected older MVCC status strings. The current Phase 4F.2 engine legitimately reports `native_dynamic_speculative_wave` and dynamic native matching instead.
3. Target package metadata still emitted `0.0.7`; release ZIP/manifest/workflow artifacts now emit `0.0.7.1`.

## Compiler warnings

The Linux Core build succeeded, but the compiler emitted approximately **178 warning lines** in the captured audit log, primarily existing fixed-size path `snprintf` truncation diagnostics. They are not build failures, but they should be treated as a hardening backlog rather than ignored indefinitely.

## How to run

Local full source/Core audit:

```bash
./scripts/final-release-audit.sh
```

Inspect all five release plans without compiling:

```bash
./scripts/build-all-targets.sh --all --plan
```

On a supported native host:

```bash
./scripts/build-all-targets.sh --target host
```

For all five native release artifacts, dispatch `.github/workflows/build-all-targets.yml` on the repository. A target is release-green only when its native build and installer packaging complete successfully.

## Release decision

**0.0.7.1 Core/source audit is green.** Do not label all five desktop installers final until the native runner matrix is green for Linux x64, Linux ARM64, macOS x64, macOS ARM64 and Windows x64.
