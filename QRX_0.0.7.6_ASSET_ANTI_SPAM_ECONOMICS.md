# QRX 0.0.7.6 – Asset Anti-Spam Economics

The native-asset burn schedule was recalibrated against the current QRX Mainnet issuance schedule.

Current initial block subsidy:
- 25,000,000 atoms = 0.25 QUB
- target block time: 10 seconds

The previous asset defaults were too cheap relative to network issuance. A MAIN namespace cost only 1 QUB, equivalent to four initial block subsidies. That made automated namespace/state spam unnecessarily inexpensive.

## New Mainnet defaults

| Operation | Burn | Initial subsidy equivalents | Approx. aggregate initial issuance time |
|---|---:|---:|---:|
| MAIN issue | 100 QUB | 400 blocks | 66m 40s |
| SUB issue | 25 QUB | 100 blocks | 16m 40s |
| UNIQUE issue | 2.5 QUB | 10 blocks | 1m 40s |
| CHANNEL issue | 25 QUB | 100 blocks | 16m 40s |
| QUALIFIER issue | 250 QUB | 1,000 blocks | 2h 46m 40s |
| SUBQUALIFIER issue | 25 QUB | 100 blocks | 16m 40s |
| RESTRICTED issue | 500 QUB | 2,000 blocks | 5h 33m 20s |
| REISSUE / remint | 10 QUB | 40 blocks | 6m 40s |
| TAG / UNTAG | 1 QUB | 4 blocks | 40s |

All amounts above are **burned QUB**. They are not paid to the Development Fund, the asset issuer, or validators. Normal transaction fees remain separate and continue under QRX fee policy.

The expensive operations are intentionally those that create scarce namespaces or durable regulated-policy state. Routine transfers do not pay an asset burn. Reissuance remains possible only when the asset was originally issued with `reissuable=1`; once changed to `0`, it cannot be re-enabled.

The defaults are consensus chain parameters and are duplicated in `fork.0` genesis parameters. They can only be changed through the height-aware protocol/governance configuration path, not by a local wallet preference.

This is an anti-spam economic barrier, not an absolute DoS guarantee. Consensus resource limits, transaction fees, block limits, mempool policy and rate controls remain necessary in addition to burns.
