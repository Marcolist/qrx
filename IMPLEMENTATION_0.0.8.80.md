# QRX 0.0.8.80 — Ad Shield + Network-Paid Sponsored Content

QRX browser security remains deny-by-default for conventional advertising. Sponsored visibility is a protocol feature, not an exception to CSP or browser isolation.

## Reward economics
Per cryptographically settled impression:
- 55%: three diverse delivery/network providers
- 25%: QRX site publisher/host
- 10%: viewer reward
- 5%: protocol reserve
- 5%: development

The viewer share is intentionally capped below infrastructure/publisher rewards because permissionless wallet creation makes large viewer-only rewards Sybil-prone. Frequency rewards are limited to one campaign/viewer-token pair per 144-block epoch. Advertisers only spend escrow on settled impressions and can reclaim unused campaign budget after expiry.

## Consensus transactions
- `AD_CAMPAIGN_CREATE`
- `AD_DELIVERY_RECEIPT`
- `AD_IMPRESSION_SETTLE`
- `AD_REWARD_CLAIM`
- `AD_CAMPAIGN_CLOSE`

A settlement requires three ACTIVE provider receipts and operator/ASN/region diversity checks. Receipt transactions are signed with each provider's normal QRX transaction identity. Replay/frequency keys are committed to QRXDB in the same WAL batch as campaign/reward state.

## Ad Shield
Paid campaigns do not obtain arbitrary browser privileges. The creative policy rejects executable HTML/JavaScript, trackers, fingerprinting and popups. QRX browser's existing zero-trust QRX sandbox remains authoritative.

## Validation
`net_phase122_ad_economy` plus all previous internal tests: 55/55 PASS.
