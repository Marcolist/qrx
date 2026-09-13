# QRX 0.0.7.7 — Phase 7.2.15
## Mainnet Consensus Red-Team Remediation

Base: Phase 7.2.14.1.
Target Mainnet activation remains 2026-09-15 16:00 UTC / 18:00 CEST.

## Security issues remediated

### 1. Cryptographic parent-chain binding
Every new proposal now contains:
- `previous_block_hash`
- `parent_state_root`

For height 1, `previous_block_hash` is the canonical Genesis hash. For later heights it must exactly equal the last finalized block hash. Verification rejects height skips, stale proposals, and proposals that do not extend the finalized head.

Consensus height is now derived from the finalized chain, not by counting `.block` files. Competing proposals therefore cannot artificially advance height.

### 2. Finalization-only authoritative block ingest
Proposal creation no longer calls `qrxdb_chain_ingest_block_file()`. A proposal is non-authoritative until it has a >2/3 certificate. QRXDB block indexing happens only after quorum in `finalize_block_cmd()`.

The finalization certificate records:
- height / round
- block hash
- previous block hash
- parent state root
- finalized QRXDB Merkle state root
- block timestamp
- yes power / total power

The next proposal binds to that finalized state root.

### 3. Exact-next-height finality
`verify_block_cmd()` and `finalize_block_cmd()` both fail closed unless:
`height == last_finalized_height + 1`.

A second finalization file for the same height is refused.

### 4. Deterministic consensus proposer
The active snapshot is sorted deterministically by validator address. A weighted proposer is derived from:
`SHA3-512("QRX-PROPOSER-v1" | genesis_hash | height | round)`
and mapped across snapshot voting power.

Both proposal creation and block verification enforce the same expected proposer. This replaces the security-sensitive assumption that any locally selected fleet signer could be a globally valid proposer.

### 5. SAFE PAUSE is consensus-effective
A safely paused validator is now excluded from validator snapshots and therefore from quorum power. It is rejected as:
- block proposer
- vote signer
- delegation target

Tombstoned and currently jailed validators are likewise rejected.
Historical slashable evidence remains enforceable.

### 6. Mainnet activation is verification-side enforced
On Mainnet, block verification now requires:
- local wall clock has reached canonical `genesis_time`
- block timestamp is not before `genesis_time`
- timestamp is not more than `QRX_MAX_FUTURE_DRIFT_SECONDS` ahead
- timestamp is not older than the finalized parent timestamp

The scheduled launch therefore is no longer merely a guard in the official proposer/voter path.

### 7. Forged-slashing red-team finding fixed
During remediation an additional critical issue was found: block double-sign recording/slashing previously happened before the block signature had been authenticated. A crafted conflicting block could therefore attempt to trigger evidence handling before signer authentication.

The order is now:
1. canonical block hash verification
2. Ed25519 public-key/address binding
3. Ed25519 signature verification
4. validator eligibility checks
5. snapshot/proposer checks
6. only then double-sign evidence recording/slashing

Unauthenticated forged blocks cannot create double-sign evidence.

### 8. Native consensus file enumeration
Vote tally/finalization and node inbox processing no longer build `find '...path...'` shell commands for consensus block/vote paths. Native Win32/POSIX directory enumeration is used instead, removing the identified path/shell-injection class from these consensus paths.

## Validation performed
- Fresh Core configure/build with `QRX_REQUIRE_PQC=OFF`, `QRX_BUILD_TESTS=ON`: PASS
- CTest: 7/7 PASS
- Phase 7.2.12 static security assertions: PASS
- Phase 7.2.13 home-validator safety assertions: PASS
- Phase 7.2.12 randomized staking adversarial model: 10,000 cases PASS
- New Phase 7.2.15 red-team consensus assertions: PASS
- ASAN + UBSAN fresh build: PASS
- ASAN + UBSAN CTest: 7/7 PASS
- Multi-target build-plan regression: 6/6 PASS

## Remaining launch caveats
This phase materially hardens the previously identified consensus flaws, but it is not a formal proof of BFT safety. Before Mainnet launch the real 50 validator addresses and five governance public keys must still be finalized, seed DNS/networking must be tested, and a multi-node dress rehearsal should exercise competing proposals, disconnect/reconnect, SAFE PAUSE/RESUME, WAL crash/restart and the exact 15 September activation boundary on separate hosts.

The custom privacy system still warrants an independent specialist cryptographic audit before enabling it as a high-value Mainnet feature.
