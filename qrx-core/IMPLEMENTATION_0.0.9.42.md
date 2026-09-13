# QRX 0.0.9.42 — AURA Zero-Config Model Fabric, Distributed Model Registry, Replication & Provider Discovery

Date: 2026-09-11
Status: implemented in Core; GUI beginner controls added; full CTest regression green.

## Goal
Make model deployment/discovery a QRX-native operation instead of asking users to understand model filenames, GGUF/MLX formats, CUDA flags, quantization or manual peer URLs. The model/version is content-addressed and signed; volatile provider availability stays off-chain and can converge through the AURA discovery plane.

## Core implementation
New module: `src/compute/qrx_aura_model_distribution.{h,c}`.

### Signed model identity and catalog
- `QrxAuraModelManifest` gives each exact model build a stable identity independent of filenames.
- Model metadata includes model/version/family/architecture, format, quantization, license id, context size and optional origin URI.
- Assets are individually content-addressed: config, tokenizer, shared weights, weight chunks, expert packs and adapters.
- Manifest commitment domain: `QRX/AURA/MODEL-MANIFEST/V1`.
- Signed catalog announcements bind publisher, sequence/validity window, manifest root and scheduler capability profile.
- Catalog domain: `QRX/AURA/MODEL-CATALOG/V1`.
- Model families are metadata. No Qwen/Kimi/DeepSeek vendor or version is hard-coded into consensus, so future generations can be published through signed catalog updates.

### Provider availability index
- Signed `QrxAuraModelAvailabilityAnnouncement` advertises which exact content-addressed assets a provider currently has.
- Provider/pod/region/endpoint, bandwidth, latency, reliability and an asset bitmap are included.
- Availability domain: `QRX/AURA/MODEL-AVAILABILITY/V1`.
- Higher-sequence announcements replace older state; stale announcements can be pruned.
- Model identity and volatile placement are deliberately separate: chain/governance can anchor catalog authority while P2P discovery carries live availability.

### Automatic replication targets
- Baseline global replica target is 14.
- Target rises with model popularity, policy priority, live demand and boot-critical assets.
- Expert packs receive additional replication headroom.
- Asset health reports provider replicas, region replicas, target, deficit and a deterministic priority score.
- Region diversity minimum is four regions before a placement is considered geographically healthy.

### Zero-config placement
`QrxAuraZeroConfigProfile` supports:
- ECO
- BALANCED
- PERFORMANCE

Default automatic cache ceilings are 10 GiB / 50 GiB / 200 GiB while preserving a local free-disk reserve. An explicit wallet budget can override the default.

The placement planner chooses under-replicated and boot-critical assets that fit the provider's advertised free model-cache capacity. This allows a small client to host only useful subsets/expert packs instead of entire huge models.

### Download source hierarchy
The deterministic source planner ranks:
1. local cache (handled before network planning),
2. LAN peer,
3. existing AURA pod peer,
4. nearby QRX provider,
5. QRX Drive,
6. explicitly permitted external origin.

Provider bandwidth/reliability/latency influence ordering inside a tier. QRX Drive is always a native fallback; external origin is last and can be disabled.

## Beginner GUI
The Resource Globe now contains **AURA Compute · Automatic**:
- Off / Automatic only on the normal path.
- Eco / Balanced / Performance.
- automatic or explicit model-cache budget.
- technical runtime/model format is not asked from the user.
- advanced section contains only optional relay information plus security/runtime explanation.
- wallet approval, secure dispatch and PQ-hybrid sessions are presented as enforced safety properties, not optional beginner toggles.

The Tauri source writes the durable `QRXAURA41` host contract under the wallet's network settings and passes `QRX_AURA_PROVIDER_CONFIG` to a newly spawned daemon. The actual daemon-side autonomous one-click package/runtime activation is intentionally scheduled for 0.0.9.45.

## Validation
New test: `compute_phase167_aura_zero_config_model_fabric`.

The test covers:
- Ed25519 signing/verification of catalog and availability announcements,
- signed catalog ingestion and dynamic model profile discovery,
- three-provider availability ingestion,
- asset-health/replica/region accounting,
- automatic disk-budget profile,
- under-replication-driven placement,
- source hierarchy ordering,
- stale availability pruning.

Full registered CTest after 0.0.9.44: **102/102 PASS**.
