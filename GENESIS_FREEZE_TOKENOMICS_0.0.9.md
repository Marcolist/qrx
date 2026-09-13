# QRX 0.0.9 Genesis Freeze — Tokenomics & 0.0.8 Internal Audit

Date: 2026-09-11

## Decision

The phrase "0.25" is implemented as the previously agreed **0.25 QUB protocol/staking reward per 10-second block**, not 0.25 percent. The canonical atom value is `25,000,000`.

QRX keeps one scheduled emission source: the block subsidy. Storage, useful compute and advertising are demand-funded transfers from explicit escrow and do not mint additional QUB.

## Protocol emission

- Block target: 10 seconds
- Genesis block subsidy: 0.25 QUB = 25,000,000 atoms
- Blocks/year at target: 3,153,600
- Initial annual gross subsidy: 788,400 QUB
- Halving interval: 12,614,400 blocks (~4 years)
- Maximum supply ceiling: 21,000,000 QUB
- Development fund: 20% year 1, 10% year 2, 5% year 3, 2% thereafter, carved from subsidy rather than added to it

### Supply-curve caveat before Genesis

With a 0.25-QUB initial subsidy and a four-year halving cadence, the idealized geometric subsidy series sums to about **6,307,200 QUB** before integer-atom rounding. Therefore **21,000,000 QUB is a hard maximum-supply ceiling, not the amount this subsidy schedule is designed to reach**. If the economic intention is Bitcoin-like convergence toward 21 million circulating QUB from protocol emission, the initial reward, halving schedule, or other pre-Genesis allocations must be redesigned before Block 0.

## Storage

Client-funded contract split:
- 97.5% provider escrow
- 2.0% resilience/repair reserve
- 0.5% development share

Proof and repair payouts are funded from contract pools. No storage mint exists.

## Compute / PoUC

Standard verified compute is paid from the owner's job escrow. Verification/challenge rewards are separately authorized in that escrow.

FastTrack surcharge:
- maximum surcharge: 25% of max compute budget
- 90.0% provider
- 9.5% network fee pool
- 0.5% development

The prior behavior that effectively routed 99.5% of the FastTrack surcharge to the network pool was corrected because it did not incentivize the provider that performs the priority work.

## Advertising

Advertiser-funded impression split:
- 55.0% delivery providers
- 25.0% publisher
- 15.0% viewer
- 4.5% protocol/network pool
- 0.5% development

## Verification

- Full registered CTest suite: 112/112 PASS
- 0.0.8 Storage/QRX-Net/Advertising subset: 51/51 PASS
- ASan+UBSan Storage/QRX-Net/Ads/phase178: 52/52 PASS
- ASan+UBSan changed Compute FastTrack settlement tests: 3/3 PASS

This is a strong internal regression/memory/UB audit. It is not a substitute for an independent third-party cryptography/security audit.


## Follow-up Genesis hardening

Native asset burns are now subsidy-relative (`block_reward_units_v1`) and `ADVERTISING_V1` is separated from `QRX_NET_V1`. The canonical Genesis memo remains unchanged and hashed into `genesis.cfg`. See `GENESIS_FREEZE_ASSET_BURNS_AD_ACTIVATION_0.0.9.md`.
