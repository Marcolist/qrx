# QRX 0.0.9 Genesis Freeze - AURA Real Model Bootstrap Completion

Date: 2026-09-11

## Why this patch exists

The 0.0.9 model fabric already had model manifests, provider discovery, QRX Drive/CAS fetch, placement and replication planning. What was still missing was the real bridge from an upstream model repository into governed QRX-native assets. This patch closes that gap before Genesis.

## Implemented

1. **Real Hugging Face-compatible origin adapter**
   - API metadata resolution
   - redirect support
   - immutable revision SHA pinning
   - fail-closed revision mismatch rejection
   - optional environment-only access token
   - gated-repository policy

2. **Resumable import into QRX CAS**
   - selected model/runtime files are enumerated deterministically
   - large weight files download with resume
   - every imported byte object is SHA3-addressed by `QrxStorageFs`
   - partial files are never announced as valid content

3. **Automatic model analysis**
   - `config.json` model type, layers, experts, top-k and context
   - tokenizer/config/runtime support files
   - `model.safetensors.index.json` parsing
   - weight-shard to layer/expert-range mapping for MoE placement

4. **QRX model artifacts**
   - `QrxAuraOriginSourceMap`
   - `QrxAuraModelManifest`
   - `QrxAuraModelBundleManifest`
   - `QrxAiModelRecord`
   - `QrxAuraExpertManifest` for MoE

5. **Automatic client fallback**
   - QRX Drive/CAS is always tried first
   - external origin fallback is permitted only from the governed manifest
   - fallback is pinned to the exact immutable upstream revision
   - downloaded bytes must match governed QRX content roots

6. **Replication execution**
   - the existing deterministic repair planner now has an execution API
   - a target-side QRX Drive/CAS transfer adapter fetches the planned content root
   - transfer succeeds only after content-addressed verification

7. **Official bootstrap specs**
   `config/aura/models/official-origin-catalog.qrx` contains data-only seed specifications for:
   - `qrx/qwen3.8-27b` -> `Qwen/Qwen3.8-27B`
   - `qrx/kimi-k2-instruct` -> `moonshotai/Kimi-K2-Instruct`
   - `qrx/kimi-k3` -> `moonshotai/Kimi-K3`
   - `qrx/deepseek-v3.2` -> `deepseek-ai/DeepSeek-V3.2`

8. **Operator utility**

```bash
qrx-aura-model-seed --catalog config/aura/models/official-origin-catalog.qrx \
  --model qrx/qwen3.8-27b --resolve-only

qrx-aura-model-seed --catalog config/aura/models/official-origin-catalog.qrx \
  --model qrx/qwen3.8-27b --cas /srv/qrx/model-cas --work /srv/qrx/model-import
```

## Trust model

The bootstrap catalog says where a candidate official model originates and what license/runtime family is expected. It does **not** itself make arbitrary upstream bytes trusted. The first seed run resolves the immutable revision and hashes the real bytes. The resulting QRX manifest/root is then subject to the existing signed model-governance/provenance path.

## Important operational limitation

Kimi K2/K3 and DeepSeek full models are extremely large. This development container did not download or mirror the complete official multi-hundred-GB/TB weight sets. Therefore this source archive does not pretend to contain pre-seeded real QRX roots for those complete models. The real seed operator must perform the one-time import with adequate storage/bandwidth before the governed manifest is published. After that, QRX providers can discover and replicate the assets without each user manually visiting Hugging Face.

## Regression result

- Complete CTest matrix: **109/109 PASS**
- New test: `compute_phase175_aura_real_model_origin_bootstrap`
- New test: `compute_phase176_aura_edge_models_zero_touch`
- The test covers immutable resolution, license policy, resumable origin import, CAS hashing, MoE index mapping, manifest generation, fresh-client origin fallback, model materialization, availability announcements, repair planning and an actual verified transfer into a second provider cache.


## Nano/Edge zero-touch completion

The V2 official model-origin catalog now has real non-gated GGUF bootstrap entries for Qwen 0.6B/1.7B/4B, DeepSeek R1 Distill Qwen 1.5B and Ministral 3B. AUTO receives per-model memory/throughput/task hints and selects the smallest useful capacity class for the request and measured machine. Native Apple Metal uses a distinct llama.cpp Metal adapter; it is not conflated with MLX. Meta Llama lives in a separate optional gated catalog and is not a first-run dependency.
