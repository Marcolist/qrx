# QRX 0.0.9.49 — A–Z Documentation, Setup, Menu Navigation & Operator Handbook

0.0.9.49 is the mandatory documentation closeout for the 0.0.9 branch.

Deliverables:
- `docs/QRX_A_TO_Z_0.0.9.md` — canonical complete offline handbook.
- setup/build/first-start instructions.
- wallet creation/unlock/recovery/migration guide.
- menu-by-menu GUI reference for every actual `view-*` page in the shipped HTML.
- QRX Drive, QRX-Net and AURA beginner/operator guides.
- CLI command-group examples checked against the native `qrx_cli.c` inventory.
- security, upgrade/migration, troubleshooting, developer map, FAQ and glossary.
- automated documentation coverage gate (`phase174`).

The release is not documentation-complete until phase174 passes against the actual GUI view inventory.

Validation:
- phase174 scans the actual GUI `view-*` inventory and requires a documentation mapping for every view.
- all mandatory setup/recovery/Drive/QRX-Net/AURA/CLI/security/troubleshooting/glossary chapters are gated.
- final full registered Core regression suite: **107/107 PASS**.

Status: **DONE — final 0.0.9 roadmap phase.**
