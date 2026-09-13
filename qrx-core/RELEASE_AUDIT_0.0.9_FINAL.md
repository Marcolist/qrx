# QRX 0.0.9 Final — Release Closeout Audit

Date: 2026-09-11

## Roadmap status

0.0.9.45 through 0.0.9.49 are complete in this source snapshot. 0.0.9.49 is the final 0.0.9 roadmap phase.

## Final block delivered

- 0.0.9.45: signed runtime packages, native hardware discovery and one-click AURA activation.
- 0.0.9.46: governed model catalog, provenance, license policy, controlled update and rollback.
- 0.0.9.47: heterogeneous WAN routing with quality/latency/cost/energy/reliability policy plus telemetry commitments.
- 0.0.9.48: migration hardening and fail-closed release-readiness checks.
- 0.0.9.49: canonical A-Z handbook and phase174 documentation/menu coverage gate.

### Genesis Freeze post-roadmap extensions

- phases 175–177: real model-origin bootstrap, Nano/Edge zero-touch and signed runtime delivery;
- phases 178–183: tokenomics, Genesis memo/asset-burn activation, governance vault and common protocol readiness;
- phases 184–186: untrusted-input/remote malformed transaction whitehat guards;
- phases 187–188: QRX Upscaler local foundation and capability profiles;
- phase 189: `.qrxapp` local App Foundation, sandbox/App Host, Mini JS SDK and demo.
- Apple Silicon Upscaler compatibility is now represented by one `apple-silicon` profile; M-generation/variant is metadata for display/Auto tuning, not a separate platform contract.

## Regression evidence

The complete registered CTest matrix was rebuilt and run after the Genesis Freeze model/runtime, governance/readiness, whitehat hardening, QRX Upscaler and QRX App Foundation extensions (through phase189):

- Total: **122**
- Passed: **122**
- Failed: **0**
- Documentation gate: PASS
- Actual GUI `view-*` pages covered by phase174: **20**

The phase174 gate reads the shipped `GUIWALLET/src/index.html` and the canonical handbook. Any new unmapped GUI view fails the test. It also requires setup, wallet recovery, QRX Drive, QRX-Net, AURA, CLI/RPC, security, troubleshooting and glossary coverage.

## Genesis Freeze AURA model-bootstrap patch

Before the Genesis freeze, the previously generic `EXTERNAL_ORIGIN` placeholder was completed as a real model bootstrap pipeline. The source now includes:

- libcurl-backed Hugging Face-compatible repository resolution;
- immutable revision pinning with fail-closed SHA matching;
- license/gating checks before import;
- resumable large-file downloads into QRX SHA3 content-addressed storage;
- config/tokenizer/Safetensors discovery and MoE expert-range extraction from the Safetensors index;
- QRX model bundle, source-map, model-registry and expert-manifest generation;
- Drive-first model fetch with governed immutable-origin fallback;
- provider catalog/availability bridge;
- executable replication plans with target-side verified QRX Drive/CAS fetch;
- `qrx-aura-model-seed` operator utility;
- data-only official bootstrap catalog entries for Qwen3.8-27B, Kimi-K2-Instruct, Kimi-K3 and DeepSeek-V3.2.

The phase175 end-to-end test uses a byte-for-byte local Hugging Face-compatible fixture, including a Safetensors index, two weight shards, immutable revision fallback, fresh-client fallback and an actual asset transfer into a second provider cache.

## Genesis Freeze Nano/Edge zero-touch patch

Phase176 extends the real bootstrap path down to single-device Nano/Edge hosts. The official origin catalog is now V2 and includes real GGUF origins/quantizations for Qwen 0.6B, Qwen 1.7B, Qwen 4B, DeepSeek-R1-Distill-Qwen 1.5B and Ministral 3B alongside the larger Qwen/Kimi/DeepSeek entries. Per-entry minimum/recommended memory, minimum measured throughput and task-quality scores allow AUTO to choose without exposing model names to beginners.

The test covers a ~3 GiB tiny host, a Pi-5-like 8 GiB host, task-specific Nano/Edge selection, Apple Metal -> llama.cpp Metal selection, a multi-quantization GGUF fixture where only the governed file filter is imported, and a separate optional gated Llama catalog that normal AUTO cannot select.

Meta Llama is intentionally optional/Advanced rather than a zero-touch dependency because its official distribution uses a custom Meta community license/access flow. The default zero-touch set uses non-gated GGUF origins that can be fetched and executed through the llama.cpp runtime path without asking a beginner to choose a model, format or quantization.

**Operational boundary:** the real upstream weights for the huge official models are not embedded in this source archive and were not physically mirrored in this build environment. Their real QRX SHA3 roots are produced by the first authorized seed/import run and then published through the governed model catalog. This is an operational seeding step, not a simulated claim in this audit.

## Phase177 runtime delivery hardening

The current tree includes the signed AURA runtime delivery matrix and daemon auto-bootstrap path. Normal users can stay on `AURA = Automatic`; QRX chooses a compatible CPU/CUDA/Metal/ARM runtime, verifies the governed publisher signature and SHA3 content root, probes the runtime ABI and only then activates it.

## Phase178 Genesis tokenomics hardening

The Mainnet initial block reward is now a shared consensus/economics constant of **25,000,000 atoms = 0.25 QUB every 10 seconds**. Storage, compute and advertising are explicitly demand-funded service layers rather than independent minting sources.

Consensus/service splits are regression-locked as follows:

- Storage: 97.5% provider budget / 2.0% resilience / 0.5% development.
- Compute FastTrack surcharge: 90.0% provider / 9.5% network fee pool / 0.5% development, capped at 25% of the job's maximum compute budget.
- Advertising: 55% delivery / 25% publisher / 15% viewer / 4.5% protocol / 0.5% development.

A previously inconsistent FastTrack settlement that sent the non-development premium to the network pool instead of the provider was corrected. A dedicated phase178 test proves conservation of funded QUB across the block, storage, compute and ad splits.

The 0.0.8 Storage/QRX-Net/Advertising subset was re-run separately at **51/51 PASS**. The same subsystem plus phase178 was also built and run under AddressSanitizer + UndefinedBehaviorSanitizer: **52/52 PASS**, with the changed FastTrack compute settlement tests also passing under sanitizers. The sanitizer exercise also exposed and fixed incomplete sanitizer linker propagation for later test targets. These are internal engineering/security checks, not an independent external audit.

## Phase179 Genesis memo, subsidy-relative asset burns & advertising gate

The canonical Genesis memo is written into `genesis.cfg` by the chain-parameter writer and therefore contributes to the Genesis hash. It remains byte-for-byte unchanged in this patch. Native-asset anti-spam burns now use `block_reward_units_v1`: the Genesis prices preserve the historic QUB amounts at the 0.25-QUB subsidy, then scale down with future subsidy halvings instead of becoming a progressively larger fraction of circulating supply. MAIN is 400 current block rewards (100 QUB at Genesis, 50 QUB after the first halving); the other asset operations are locked to their corresponding reward-unit multipliers.

Advertising activation is now separated from `QRX_NET_V1` by a dedicated fail-closed `ADVERTISING_V1` feature flag. `ADVERTISING_V1` additionally requires QRX-Net to be active, including inside the consensus preparation path, so direct callers cannot bypass the outer transaction gate. Operational target dates are metadata/readiness targets only; consensus activation remains threshold-governed by explicit block height.

Phase179 regression coverage verifies the exact canonical memo, dynamic asset-burn derivation across the first halving, the nonzero anti-spam floor after subsidy exhaustion, independent QRX-Net/Advertising activation, direct consensus bypass resistance, and the staged target metadata. The post-patch full matrix is **113/113 PASS**. Targeted ASan+UBSan coverage for the changed resource/net/genesis paths is **3/3 PASS**.

## Documentation deliverables

- `docs/QRX_A_TO_Z_0.0.9.md` — canonical source.
- `docs/QRX_A_TO_Z_0.0.9.html` — offline searchable/browser copy.
- `docs/QRX_A_TO_Z_0.0.9.pdf` — printable A4 copy.
- `QRX_A_TO_Z_DOCUMENTATION_PLAN_0.0.9.49.md` — documentation scope/gate plan.

The PDF was rendered to images and visually checked for clipping/layout issues on representative first, middle and final pages.

## Security/release boundary

This closeout is not an independent security audit or a claim that every target installer was produced in this Linux container. The following remain release-host/CI operations:

- native macOS/Windows/Linux Tauri builds for every advertised architecture;
- Apple/Windows signing and notarization where required;
- final public-distribution key/signature ceremony;
- external security review where desired.

The source tree keeps these limitations explicit rather than treating a C regression pass as installer or security certification.

## Result

**0.0.9 roadmap status: GENESIS FREEZE CANDIDATE / source closeout + real model-bootstrap patch complete.**

Next roadmap branch: **0.0.10 — Ouroboros / QRX DAO & AURA governance evolution.**


## Genesis tokenomics caveat

The Genesis reward baseline is now 0.25 QUB per 10-second block. Storage, useful compute, and advertising are user/advertiser-funded and do not create additional scheduled minting. With four-year halvings, the idealized subsidy series is approximately 6,307,200 QUB before integer-atom rounding, so the 21,000,000-QUB value is currently a hard ceiling rather than an emission target. This is a conscious economics decision that must be accepted or revised before Block 0.


## Phase 180 - Governance Vault V1

Operator governance is hardened without changing the 3-of-5 consensus threshold. The offline backup vault can hold all five independently encrypted governance roots but is non-signing and stores private material as `governance.key.backup`. The operational vault permits at most two private signing roots; the other three entries are public-descriptor-only. A valid third signature can be produced on a separate offline signer and imported into the unchanged `governance-apply` threshold verifier. The integration regression explicitly rejects a third online signer and verifies a 2-online + 1-offline 3-of-5 apply path.
