# QRX 0.0.7.7 Phase 7.2 — Genesis Governance & KYC Provider Registry

## Release goal
Phase 7.2 reduces final Mainnet source input to two public datasets:
1. exactly 50 bootstrap validator QRX addresses;
2. exactly five Ed25519 developer-governance public keys (3-of-5 threshold).

No KYC provider is hard-coded into Genesis. Providers can be approved, disabled, rotated or updated after Genesis by 3-of-5 developer governance.

## Genesis-bound governance
Mainnet `genesis.cfg` now commits:
- `governance_threshold=3`
- `governance_root_count=5`
- each `DEV_GOV_1..5` public key
- 50 bootstrap validator addresses
- 1000 QUB self-stake per bootstrap validator
- 180-day block-height lock
- staking allowed during lock; generic release/unstake before unlock forbidden

Changing any validator address or governance public key changes the Genesis hash and Chain ID.
Mainnet rejects legacy `governance-genesis-init`; runtime root files cannot replace Genesis roots.

## KYC Provider Registry
Consensus governance actions:
- `KYC_PROVIDER_ADD`
- `KYC_PROVIDER_DISABLE`
- `KYC_PROVIDER_ROTATE_KEY`
- `KYC_PROVIDER_UPDATE`

Provider QRXDB state includes provider ID, ACTIVE/DISABLED status, QRX authority address, Ed25519 public key, capabilities and last governance TX.

A governance-bound qualifier uses `#KYC_<PROVIDER_ID>`, e.g. `#KYC_CURA`.
Only the ACTIVE provider's registered QRX authority may issue/tag its KYC qualifier. Generic transfers of `#KYC_*` authority are rejected. When a provider is disabled, its existing KYC tags stop satisfying restricted-asset verifier expressions immediately.

KYC/AML documents and PII remain off-chain. QRX stores provider authorization and eligibility tags/credentials, not identity documents.

## Final input helper
Create `validators.txt` from `qrx-core/config/mainnet/bootstrap-validators.txt.template`, then run:

```
python3 scripts/finalize-mainnet-genesis-inputs.py \
  --validators validators.txt \
  --governance mainnet-governance-keys/DEV_GOV_1/governance.pub \
               mainnet-governance-keys/DEV_GOV_2/governance.pub \
               mainnet-governance-keys/DEV_GOV_3/governance.pub \
               mainnet-governance-keys/DEV_GOV_4/governance.pub \
               mainnet-governance-keys/DEV_GOV_5/governance.pub
```

The script reads public material only, validates counts/duplicates/fingerprints and writes the two Genesis source arrays.

## Seed nodes
The Mainnet profile uses `seed1.qrxchain.org:26660`, `seed2.qrxchain.org:26660`, and `seed3.qrxchain.org:26660`. These hostnames are not Genesis keys; operational DNS must point them to reachable QRX nodes before public launch.

## Validation performed
- fresh CMake Core/qrxd build: PASS
- CTest VELOCITY/MVCC/SPV: 7/7 PASS
- Phase 7.1 privacy governance regression: PASS
- Phase 7.1 privacy consensus transaction regression: PASS
- Phase 7.2 KYC registry E2E: PASS
- synthetic Mainnet Genesis with 50 validators + 5 real generated governance roots: PASS
- 3-of-5 governance resolved directly from Genesis with no `governance_roots.db`: PASS
- placeholder Mainnet interlock: PASS (refuses initialization)

## Mainnet state
The source archive intentionally retains validator/governance placeholders. Mainnet stays locked until the operator supplies the final 50 validator addresses and five public governance roots and recompiles.
