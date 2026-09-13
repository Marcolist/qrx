# QRX 0.0.9.43 — Autonomous Model Placement, Replication Repair & Hot Expert Migration

Date: 2026-09-11
Status: implemented and regression-tested.

## Goal
Turn discovery into self-healing placement. QRX should not merely know that an expert is missing or overloaded; it should deterministically decide what to replicate, from which healthy source, and onto which compatible provider/region.

## Core implementation
New module: `src/compute/qrx_aura_model_autoplacement.{h,c}`.

### Inputs
- signed model manifest/catalog state from 0.0.9.42,
- live provider availability index,
- per-asset demand/activation/latency statistics,
- candidate provider capacities and role masks.

### Repair reasons
- `UNDER_REPLICATED`: provider count is below deterministic target.
- `REGION_GAP`: geographic diversity is below policy floor.
- `HOT_EXPERT`: an expert pack is experiencing high routing demand and benefits from migration/replication closer to demand.

### Placement rules
- never choose a target that already advertises the asset,
- require sufficient model-cache capacity,
- require a provider role capable of model-cache/inference work,
- strongly prefer a missing region when repairing region diversity,
- choose a stable source using reliability, bandwidth and latency,
- deterministic tie breaks prevent planner disagreement for equal inputs,
- repair items are priority-sorted so boot-critical/deficit/hot assets are repaired first.

The result is a bounded repair plan. Execution/downloading remains separated from planning so storage/payment/security layers can authorize and account for the transfer.

## Validation
New test: `compute_phase168_aura_autoplacement_repair_hot_expert`.

The test proves:
- an under-replicated asset creates a repair action,
- a hot expert creates a migration/replication action,
- a missing region is preferred when a compatible destination exists,
- source/target selection is deterministic.

Full registered CTest after 0.0.9.44: **102/102 PASS**.
