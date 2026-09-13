# QRX 0.0.8 Internal Audit - Storage, QRX-Net and Advertising

Date: 2026-09-11
Scope: 0.0.8 resource foundation, QRX Drive/storage, QRX-Net/domain/browser/hosting/advertising paths, plus the Genesis tokenomics policy that constrains service economics.

## Result

- Normal regression subset: **51/51 PASS**
- ASan + UBSan subset including Genesis tokenomics policy: **52/52 PASS**
- Full current 0.0.9 Core regression suite after Genesis activation hardening: **112/112 PASS**
- Changed PoUC/FastTrack settlement paths under ASan + UBSan: **3/3 PASS**

Evidence is stored under `audit/` in this source tree.

## Mainnet activation posture

0.0.8 resource features remain activation-gated/fail-closed on Mainnet until the scheduled protocol activation. Passing these tests does not bypass the activation gate.

## Tokenomics invariants checked

- Protocol subsidy starts at 0.25 QUB per 10-second block.
- Storage contracts are client-funded and do not mint QUB.
- Useful-compute jobs are user-escrow-funded and do not mint QUB.
- Advertising campaigns are advertiser-funded and do not mint QUB.
- All tested service splits conserve the funded amount.
- Development shares are splits of existing subsidy/escrow, not additive issuance.

## Important limitation

This is an **internal regression, memory-safety and undefined-behavior audit**. It is not an independent third-party security or cryptography audit. Privacy/cryptography release gates and the final Genesis dress rehearsal remain separate release requirements.
