# QRX 0.0.7.7 Phase 7.2.12 — Security Audit Remediation & Adversarial Consensus Hardening

Phase 7.2.12 closes the critical/high findings from the adversarial 7.2.11 audit. The security rule is now: staking/delegation state changes are network transactions, not local administrative mutations.

## 1. Legacy direct staking/delegation mutations disabled
The old `stake`, `unstake`, `claim-unbonded`, `delegate`, `undelegate` and `claim-undelegated` direct state mutation paths fail closed. They cannot alter the authoritative state. Wallets must create, sign and broadcast consensus-native transactions.

## 2. GUI uses signed consensus transactions
The Tauri commands use the VELOCITY transaction envelope and broadcast `STAKE_BOND`, `DELEGATE_BOND`, `DELEGATE_UNBOND` and `DELEGATE_CLAIM`. The transaction is chain/genesis/protocol bound, lane-nonced, signed by the wallet and accepted through normal transaction processing.

## 3. Claim double-credit/mint bug removed
The old persistent `staking:claim_credit:*` accumulator is removed. Claimable principal is calculated from the matured unbonding/undelegating record, credited exactly once in the same atomic batch, and its pending/maturity records are cleared in that batch. A second claim is rejected.

## 4. Staking/delegation consensus validation
Delegation targets must be eligible validators: sufficient authoritative QRXDB self-stake, not jailed and not tombstoned. Validator power and staking status read authoritative QRXDB state first. Bootstrap self-stake remains subject to its Genesis principal floor semantics.

## 5–6. Local RPC token + browser/CSRF hardening
`qrxd` creates a cryptographically random 256-bit local RPC session token in `chain/rpc.token` (0600 on POSIX). `qrx-cli` reads and sends the token as `X-QRX-RPC-Token`. `/rpc` is POST-only. Host, Origin and `Content-Type: application/json` are validated. A missing/invalid token returns 401; rejected browser origin/content type returns 403. Basic RPC credentials remain an optional additional layer.

## 7–8. Daemon-confirmed unlock + per-signer Validator Fleet sessions
Encrypted wallet readiness is not declared merely because the GUI decrypted a file: the daemon signer session must accept the wallet-specific session. Validator Fleet signer secrets are held per wallet in daemon memory, not in one shared global signer secret. POSIX signer subprocesses receive their passphrase only in the child process immediately before exec. `walletlockfor` removes a single signer session without locking the whole fleet.

## 9. Tauri hardening
CSP is enabled instead of `csp: null`. Direct Tauri filesystem access is no longer scoped over the complete user HOME; it is limited to application-local/resource locations. Existing Rust backend file-selection/import workflows remain explicit backend operations.

## 10. Sanitizers and adversarial tests
The release includes Phase 7.2.12 security assertions, a 10,000-case randomized staking accounting model, malformed signed-transaction mutation checks, replay rejection, double-claim rejection, ASAN/UBSAN validation and the existing VELOCITY/SPV regression suite.

## 11. Supply invariant
New command:

    qrx supply-invariant <chain-dir>

It checks the conservation equation against authoritative QRXDB categories:

    minted_supply - burned_supply
      == spendable balances + bonded principal + unbonding principal + protocol pools

Delegated-total mirrors are deliberately not double-counted in the invariant.

## 12. Atomic WAL crash recovery
The Phase 7.2.12 crash test injects a process crash immediately after QRXDB WAL commit while applying a staking transaction. Reopening the database must recover exactly the normal state root, preserve the bonded stake, and reject replay of the already-applied transaction.

## Important operational semantics retained
- Validator Mode ON does not move or stake QUB.
- Normal validator identities require the protocol minimum self-stake chosen by the user.
- Bootstrap identities already have their 1,000 QUB Genesis self-stake.
- The 180-day Bootstrap lock applies to Genesis principal, not later rewards/incoming QUB.
- Bootstrap liveness grace never disables double-sign protection.
- One `qrxd` may operate multiple Validator Fleet signer identities; each identity has its own signer session.

## Security posture
This phase materially closes the findings from the internal adversarial audit, but it is not a substitute for an independent third-party pre-Mainnet cryptography/consensus audit. The production platform builders continue to require PQC; local test builds may use `QRX_REQUIRE_PQC=OFF` only to exercise logic in constrained CI/audit environments.
