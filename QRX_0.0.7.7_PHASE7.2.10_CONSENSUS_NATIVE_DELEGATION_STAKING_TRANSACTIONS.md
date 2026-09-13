# QRX 0.0.7.7 Phase 7.2.10 — Consensus-Native Delegation & Staking Transactions

## UX rule
Enabling Validator Mode never stakes or transfers QUB. It only permits that wallet identity to be used by the validator runtime.
A normal validator address additionally needs the protocol minimum self-stake collateral.
A bootstrap address already owns its 1,000 QUB Genesis self-stake, so it does not need a second collateral transaction merely to become eligible.

## Consensus-native transaction families
- STAKE_BOND
- STAKE_UNBOND
- STAKE_CLAIM
- DELEGATE_BOND
- DELEGATE_UNBOND
- DELEGATE_CLAIM

They use the signed VELOCITY transaction envelope, chain/genesis/protocol binding, lane nonce and expiry rules. Staking/delegation state is staged in the same QRXDB atomic batch as balances, nonce, fee accounting, applied-tx marker and transaction index. QRXDB/WAL is authoritative.

## Bootstrap semantics retained
The Genesis bootstrap principal remains the 180-day minimum self-stake floor. Rewards and later incoming QUB are not part of that floor. Extra self-stake above the floor can be unbonded. Bootstrap liveness grace does not suppress double-sign slashing.

## Delegation
Delegation does not give the validator the delegator private key and does not require the delegator machine to remain online. Undelegation enters the protocol unbonding period before claim.
