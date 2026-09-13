# QRX 0.0.8.2 — Capacity Accounting & Redundancy Metrics Foundation

## Purpose
QRX Drive must never present a misleading single "network storage" number. This phase introduces the canonical accounting model that later Provider Registry, QRXScan and GUI Resource Globe use.

## Separate capacity classes
The model distinguishes:
- Configured Capacity — operator `max_usage` quota.
- Raw Eligible Capacity — min(configured quota, locally eligible physical capacity after reserve rules).
- Proven Capacity — capacity accepted by future Capacity Proof.
- Allocated Physical Capacity — actual shard bytes stored for contracts.
- Logical User Data — user-visible pre-redundancy data size, counted once at contract/network level.
- Free Proven Physical Capacity — proven minus allocated.
- Available Usable Capacity — free physical capacity converted through the selected redundancy profile.
- Usable Capacity — logical data already stored plus available usable capacity.
- Reserve Committed Capacity — free proven bytes already reserved for pending placements.

## Why logical data is not summed per provider
With erasure coding, many providers hold shards of the same logical object. Summing a provider-level logical value would double-count the same user data. Therefore `logical_user_bytes` is supplied from contract-level state exactly once.

## Redundancy-aware capacity
For STANDARD 10+4:

`available_usable = free_proven_physical * 10 / 14`

For FAST 3x full replication:

`available_usable = free_proven_physical / 3`

ARCHIVE uses its configured data/parity ratio.

## Network metrics produced
- provider count
- proven-provider count
- configured bytes
- raw eligible bytes
- proven bytes
- allocated physical bytes
- logical user bytes
- free proven physical bytes
- available usable bytes
- total usable capacity
- reserved bytes
- utilization bps
- resilience headroom bps
- actual redundancy multiplier (`1.40x == 14000`)
- healthy shards
- degraded shards
- repairing shards

These fields directly support the planned GUI/QRXScan displays:

`Raw / Proven / Usable / Allocated / Available / Redundancy / Health`

## Fail-closed invariants
A provider capacity record is rejected if:
- proven > raw eligible
- allocated > proven
- pending reserve > proven free capacity
- availability > 100%
- proof success > 100%
- counters overflow during network aggregation

## Validation
The unit test covers three providers with different configured/physical/proven/allocated values and verifies:
- aggregate accounting
- 10+4 usable-capacity conversion
- 3x FAST conversion
- utilization
- resilience headroom
- actual redundancy ratio
- shard-health counters
- invalid capacity claim rejection

Full CTest after this phase: 10/10 PASS.
