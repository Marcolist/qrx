#include "compute/qrx_aura_model_fabric.h"

#include <limits.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GIB (1024ULL * 1024ULL * 1024ULL)

static int bounded_nonempty(const char *s, size_t n) {
    return s && memchr(s, '\0', n) && s[0];
}

static int bps_ok(uint32_t v) {
    return v <= 10000u;
}

static int hex64_or_empty(const char *s) {
    if (!s || !memchr(s, '\0', 65)) return 0;
    if (!s[0]) return 1;
    if (strlen(s) != 64) return 0;
    for (size_t i = 0; i < 64; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static uint64_t sat_add64(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

static uint32_t sat_add32(uint32_t a, uint32_t b) {
    return UINT32_MAX - a < b ? UINT32_MAX : a + b;
}

static uint32_t ratio_bps64(uint64_t have, uint64_t need) {
    if (!need || have >= need) return 10000u;
    if (have <= UINT64_MAX / 10000ULL) {
        return (uint32_t)((have * 10000ULL) / need);
    }

    /* Exact integer fallback without floating point or 128-bit arithmetic.
       For candidate bps s, have/need >= s/10000 iff
       have >= ceil(need*s/10000).  Split need into q*10000+r so the
       threshold can be formed without overflowing uint64_t. */
    uint32_t lo = 0, hi = 9999;
    const uint64_t q = need / 10000ULL;
    const uint64_t r = need % 10000ULL;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo + 1u) / 2u;
        const uint64_t tail = (r * (uint64_t)mid + 9999ULL) / 10000ULL;
        const uint64_t threshold = q * (uint64_t)mid + tail;
        if (have >= threshold) lo = mid;
        else hi = mid - 1u;
    }
    return lo;
}

static uint32_t ratio_bps32(uint32_t have, uint32_t need) {
    if (!need || have >= need) return 10000u;
    return (uint32_t)(((uint64_t)have * 10000ULL) / need);
}

int qrx_aura_pod_capacity_validate(const QrxAuraPodCapacity *p) {
    if (!p || p->version != QRX_AURA_FABRIC_VERSION) return -1;
    if (!bounded_nonempty(p->provider_id, sizeof(p->provider_id)) ||
        !bounded_nonempty(p->pod_id, sizeof(p->pod_id)) ||
        !bounded_nonempty(p->region, sizeof(p->region))) return -2;
    if (p->backend < QRX_MOE_BACKEND_CPU || p->backend > QRX_MOE_BACKEND_OTHER) return -3;
    if (!p->role_mask || (p->role_mask & ~QRX_AURA_POD_ROLE_ALL)) return -4;
    if (!p->usable_memory_bytes || p->free_memory_bytes > p->usable_memory_bytes || !p->node_count) return -5;
    if (!bps_ok(p->utilization_bps) || !bps_ok(p->reliability_bps) ||
        !bps_ok(p->cache_hit_bps) || !bps_ok(p->expert_locality_bps)) return -6;
    if (!hex64_or_empty(p->calibration_commitment)) return -7;
    if ((p->role_mask & QRX_AURA_POD_ROLE_INFERENCE) &&
        (!p->measured_milli_tokens_per_second || !p->max_context_tokens ||
         !p->calibration_commitment[0])) return -8;
    if (p->available > 1) return -9;
    return 0;
}

int qrx_aura_model_profile_validate(const QrxAuraModelCapabilityProfile *m) {
    if (!m || m->version != QRX_AURA_FABRIC_VERSION) return -1;
    if (!bounded_nonempty(m->model_id, sizeof(m->model_id)) ||
        !bounded_nonempty(m->model_version, sizeof(m->model_version)) ||
        !bounded_nonempty(m->family, sizeof(m->family)) ||
        !bounded_nonempty(m->runtime_id, sizeof(m->runtime_id)) ||
        !bounded_nonempty(m->quantization, sizeof(m->quantization))) return -2;
    if (m->tier < QRX_AURA_TIER_NANO || m->tier > QRX_AURA_TIER_K3_CLASS) return -3;
    if (!m->min_memory_bytes || m->recommended_memory_bytes < m->min_memory_bytes ||
        !m->min_storage_bytes || !m->min_aggregate_milli_tokens_per_second ||
        !m->min_pods || !m->max_context_tokens) return -4;
    if (!bps_ok(m->min_reliability_bps) || !bps_ok(m->min_cache_hit_bps) ||
        !bps_ok(m->min_expert_locality_bps) || !bps_ok(m->quality_bps) ||
        !bps_ok(m->coding_bps) || !bps_ok(m->reasoning_bps) ||
        !bps_ok(m->rag_bps) || !bps_ok(m->tool_use_bps)) return -5;
    if (!m->quality_bps) return -6;
    if (m->is_moe && m->tier < QRX_AURA_TIER_CLUSTER) return -7;
    if (m->supports_single_device && m->min_pods > 1) return -8;
    return 0;
}

const char *qrx_aura_tier_name(QrxAuraModelTier tier) {
    switch (tier) {
        case QRX_AURA_TIER_NANO: return "NANO";
        case QRX_AURA_TIER_EDGE: return "EDGE";
        case QRX_AURA_TIER_LOCAL: return "LOCAL";
        case QRX_AURA_TIER_CLUSTER: return "CLUSTER";
        case QRX_AURA_TIER_K2_CLASS: return "K2_CLASS";
        case QRX_AURA_TIER_K3_CLASS: return "K3_CLASS";
        default: return "NONE";
    }
}

QrxAuraModelTier qrx_aura_pod_tier_classify(const QrxAuraPodCapacity *p) {
    if (qrx_aura_pod_capacity_validate(p) || !p->available) return QRX_AURA_TIER_NONE;

    /* Utility-only nodes remain useful even if token generation is too slow.
       This is the path for small ARM/x86 SBCs doing routing, pre/post work,
       verification or cache service instead of full LLM inference. */
    if (!(p->role_mask & QRX_AURA_POD_ROLE_INFERENCE)) {
        return p->free_memory_bytes >= 2 * GIB ? QRX_AURA_TIER_NANO : QRX_AURA_TIER_NONE;
    }

    if ((p->node_count >= 2 || p->free_memory_bytes >= 32 * GIB) &&
        p->measured_milli_tokens_per_second >= 20000ULL) return QRX_AURA_TIER_CLUSTER;
    if (p->free_memory_bytes >= 16 * GIB &&
        p->measured_milli_tokens_per_second >= 8000ULL) return QRX_AURA_TIER_LOCAL;
    if (p->free_memory_bytes >= 6 * GIB &&
        p->measured_milli_tokens_per_second >= 2000ULL) return QRX_AURA_TIER_EDGE;
    if (p->free_memory_bytes >= 2 * GIB &&
        p->measured_milli_tokens_per_second >= 250ULL) return QRX_AURA_TIER_NANO;
    return QRX_AURA_TIER_NONE;
}

int qrx_aura_pod_registry_init(QrxAuraPodCapacityRegistry *r) {
    if (!r) return -1;
    memset(r, 0, sizeof(*r));
    r->version = QRX_AURA_FABRIC_VERSION;
    return 0;
}

const QrxAuraPodCapacity *qrx_aura_pod_registry_find(const QrxAuraPodCapacityRegistry *r,
                                                     const char *pod_id) {
    if (!r || r->version != QRX_AURA_FABRIC_VERSION || !pod_id || !pod_id[0]) return NULL;
    for (uint32_t i = 0; i < r->count; i++) {
        if (!strcmp(r->pods[i].pod_id, pod_id)) return &r->pods[i];
    }
    return NULL;
}

int qrx_aura_pod_registry_upsert(QrxAuraPodCapacityRegistry *r, const QrxAuraPodCapacity *p) {
    if (!r || r->version != QRX_AURA_FABRIC_VERSION || qrx_aura_pod_capacity_validate(p)) return -1;
    for (uint32_t i = 0; i < r->count; i++) {
        if (!strcmp(r->pods[i].pod_id, p->pod_id)) {
            r->pods[i] = *p;
            r->revision++;
            return 0;
        }
    }
    if (r->count >= QRX_AURA_FABRIC_LOCAL_REGISTRY_MAX_PODS) return -2;
    r->pods[r->count++] = *p;
    r->revision++;
    return 0;
}

int qrx_aura_pod_registry_remove(QrxAuraPodCapacityRegistry *r, const char *pod_id) {
    if (!r || r->version != QRX_AURA_FABRIC_VERSION || !pod_id || !pod_id[0]) return -1;
    for (uint32_t i = 0; i < r->count; i++) {
        if (strcmp(r->pods[i].pod_id, pod_id)) continue;
        for (uint32_t j = i + 1; j < r->count; j++) r->pods[j - 1] = r->pods[j];
        memset(&r->pods[--r->count], 0, sizeof(r->pods[0]));
        r->revision++;
        return 0;
    }
    return 1;
}

int qrx_aura_pod_from_calibration(const char *provider_id, const char *pod_id, const char *region,
                                  const QrxMoeCalibrationProfile *c, uint32_t role_mask,
                                  uint64_t free_memory_bytes, uint64_t model_cache_free_bytes,
                                  uint32_t network_egress_mbps, uint32_t latency_ms,
                                  uint32_t utilization_bps, uint32_t reliability_bps,
                                  uint32_t cache_hit_bps, uint32_t expert_locality_bps,
                                  uint32_t max_context_tokens, uint32_t node_count,
                                  QrxAuraPodCapacity *out) {
    if (!out || !provider_id || !pod_id || !region || qrx_moe_calibration_profile_validate(c)) return -1;
    if (strlen(provider_id) >= sizeof(out->provider_id) || strlen(pod_id) >= sizeof(out->pod_id) ||
        strlen(region) >= sizeof(out->region)) return -1;

    memset(out, 0, sizeof(*out));
    out->version = QRX_AURA_FABRIC_VERSION;
    snprintf(out->provider_id, sizeof(out->provider_id), "%s", provider_id);
    snprintf(out->pod_id, sizeof(out->pod_id), "%s", pod_id);
    snprintf(out->region, sizeof(out->region), "%s", region);
    out->backend = c->device.backend;
    out->accelerator_features = c->device.accelerator_features;
    out->role_mask = role_mask;
    out->usable_memory_bytes = c->device.device_memory_bytes;
    out->free_memory_bytes = free_memory_bytes > out->usable_memory_bytes ? out->usable_memory_bytes : free_memory_bytes;
    out->model_cache_free_bytes = model_cache_free_bytes;
    out->measured_milli_tokens_per_second = c->model_milli_tokens_per_second;
    out->network_egress_mbps = network_egress_mbps;
    out->latency_ms = latency_ms;
    out->utilization_bps = utilization_bps;
    out->reliability_bps = reliability_bps;
    out->cache_hit_bps = cache_hit_bps;
    out->expert_locality_bps = expert_locality_bps;
    out->max_context_tokens = max_context_tokens;
    out->node_count = node_count;
    out->available = 1;
    if (qrx_moe_calibration_commitment(c, out->calibration_commitment)) return -2;
    return qrx_aura_pod_capacity_validate(out);
}

static int accelerator_ok(const QrxAuraPodCapacity *p, const QrxAuraModelCapabilityProfile *m) {
    return (p->accelerator_features & m->required_accelerator_features) == m->required_accelerator_features;
}

static int basic_pod_eligible(const QrxAuraPodCapacity *p, const QrxAuraModelCapabilityProfile *m) {
    if (!p->available || !(p->role_mask & QRX_AURA_POD_ROLE_INFERENCE) || !accelerator_ok(p, m)) return 0;
    if (p->reliability_bps < m->min_reliability_bps) return 0;
    if (m->max_latency_ms && p->latency_ms > m->max_latency_ms) return 0;
    if (m->min_network_mbps && p->network_egress_mbps < m->min_network_mbps) return 0;
    if (p->max_context_tokens < m->max_context_tokens) return 0;
    return 1;
}

static int single_candidate_better(const QrxAuraPodCapacity *candidate, uint32_t candidate_score,
                                   const QrxAuraPodCapacity *best, uint32_t best_score) {
    if (!best) return 1;
    if (candidate_score != best_score) return candidate_score > best_score;
    if (candidate->measured_milli_tokens_per_second != best->measured_milli_tokens_per_second)
        return candidate->measured_milli_tokens_per_second > best->measured_milli_tokens_per_second;
    if (candidate->free_memory_bytes != best->free_memory_bytes)
        return candidate->free_memory_bytes > best->free_memory_bytes;
    if (candidate->reliability_bps != best->reliability_bps)
        return candidate->reliability_bps > best->reliability_bps;
    if (candidate->latency_ms != best->latency_ms)
        return candidate->latency_ms < best->latency_ms;
    if (candidate->cache_hit_bps != best->cache_hit_bps)
        return candidate->cache_hit_bps > best->cache_hit_bps;
    if (candidate->expert_locality_bps != best->expert_locality_bps)
        return candidate->expert_locality_bps > best->expert_locality_bps;
    return strcmp(candidate->pod_id, best->pod_id) < 0;
}

static const QrxAuraPodCapacity *single_device_best(const QrxAuraModelCapabilityProfile *m,
                                                     const QrxAuraPodCapacity *pods,
                                                     size_t pod_count,
                                                     uint32_t *readiness_out) {
    const QrxAuraPodCapacity *best = NULL;
    uint32_t best_score = 0;
    for (size_t i = 0; i < pod_count; i++) {
        const QrxAuraPodCapacity *p = &pods[i];
        if (qrx_aura_pod_capacity_validate(p) || !basic_pod_eligible(p, m)) continue;
        uint32_t s = ratio_bps64(p->free_memory_bytes, m->min_memory_bytes);
        s = min_u32(s, ratio_bps64(p->measured_milli_tokens_per_second,
                                   m->min_aggregate_milli_tokens_per_second));
        s = min_u32(s, ratio_bps32(p->cache_hit_bps, m->min_cache_hit_bps));
        if (single_candidate_better(p, s, best, best_score)) {
            best = p;
            best_score = s;
        }
    }
    if (readiness_out) *readiness_out = best_score;
    return best;
}

static uint32_t single_device_readiness(const QrxAuraModelCapabilityProfile *m,
                                        const QrxAuraPodCapacity *pods, size_t pod_count,
                                        uint64_t *selected_tps_out) {
    uint32_t readiness = 0;
    const QrxAuraPodCapacity *best = single_device_best(m, pods, pod_count, &readiness);
    if (selected_tps_out) *selected_tps_out = best ? best->measured_milli_tokens_per_second : 0;
    return readiness;
}

uint32_t qrx_aura_model_readiness_bps(const QrxAuraModelCapabilityProfile *m,
                                      const QrxAuraPodCapacity *pods, size_t pod_count,
                                      uint32_t *eligible_pods_out,
                                      uint64_t *aggregate_milli_tokens_per_second_out) {
    if (eligible_pods_out) *eligible_pods_out = 0;
    if (aggregate_milli_tokens_per_second_out) *aggregate_milli_tokens_per_second_out = 0;
    if (qrx_aura_model_profile_validate(m) || (pod_count && !pods)) return 0;

    if (!m->is_moe && m->supports_single_device) {
        uint64_t selected_tps = 0;
        uint32_t readiness = single_device_readiness(m, pods, pod_count, &selected_tps);
        if (readiness) {
            if (eligible_pods_out) *eligible_pods_out = 1;
            if (aggregate_milli_tokens_per_second_out) *aggregate_milli_tokens_per_second_out = selected_tps;
        }
        return readiness;
    }

    uint32_t eligible = 0;
    uint64_t memory = 0;
    uint64_t tps = 0;
    uint32_t network_sum = 0;
    uint32_t reliability_sum = 0;
    uint32_t cache_sum = 0;
    uint32_t locality_sum = 0;

    for (size_t i = 0; i < pod_count; i++) {
        if (qrx_aura_pod_capacity_validate(&pods[i]) || !basic_pod_eligible(&pods[i], m)) continue;
        eligible++;
        memory = sat_add64(memory, pods[i].free_memory_bytes);
        tps = sat_add64(tps, pods[i].measured_milli_tokens_per_second);
        network_sum = sat_add32(network_sum, pods[i].network_egress_mbps);
        reliability_sum = sat_add32(reliability_sum, pods[i].reliability_bps);
        cache_sum = sat_add32(cache_sum, pods[i].cache_hit_bps);
        locality_sum = sat_add32(locality_sum, pods[i].expert_locality_bps);
    }

    if (eligible_pods_out) *eligible_pods_out = eligible;
    if (aggregate_milli_tokens_per_second_out) *aggregate_milli_tokens_per_second_out = tps;
    if (!eligible) return 0;

    uint32_t avg_network = network_sum / eligible;
    uint32_t avg_reliability = reliability_sum / eligible;
    uint32_t avg_cache = cache_sum / eligible;
    uint32_t avg_locality = locality_sum / eligible;
    uint32_t score = 10000u;
    score = min_u32(score, ratio_bps64(memory, m->min_memory_bytes));
    score = min_u32(score, ratio_bps64(tps, m->min_aggregate_milli_tokens_per_second));
    score = min_u32(score, ratio_bps32(eligible, m->min_pods));
    score = min_u32(score, ratio_bps32(avg_network, m->min_network_mbps));
    score = min_u32(score, ratio_bps32(avg_reliability, m->min_reliability_bps));
    score = min_u32(score, ratio_bps32(avg_cache, m->min_cache_hit_bps));
    if (m->is_moe) score = min_u32(score, ratio_bps32(avg_locality, m->min_expert_locality_bps));
    return score;
}

static uint32_t best_tier_readiness(QrxAuraModelTier tier,
                                    const QrxAuraModelCapabilityProfile *models, size_t model_count,
                                    const QrxAuraPodCapacity *pods, size_t pod_count) {
    uint32_t best = 0;
    for (size_t i = 0; i < model_count; i++) {
        if (qrx_aura_model_profile_validate(&models[i]) || models[i].tier != tier) continue;
        uint32_t readiness = qrx_aura_model_readiness_bps(&models[i], pods, pod_count, NULL, NULL);
        if (readiness > best) best = readiness;
    }
    return best;
}

static int provider_seen_before(const QrxAuraPodCapacity *pods, size_t upto, size_t idx,
                                const char *region_or_null) {
    for (size_t i = 0; i < upto; i++) {
        if (strcmp(pods[i].provider_id, pods[idx].provider_id)) continue;
        if (!region_or_null || !strcmp(pods[i].region, region_or_null)) return 1;
    }
    return 0;
}

static int pod_seen_before(const QrxAuraPodCapacity *pods, size_t upto, size_t idx) {
    for (size_t i = 0; i < upto; i++) {
        if (!strcmp(pods[i].pod_id, pods[idx].pod_id)) return 1;
    }
    return 0;
}

static void cell_add(QrxAuraGlobeCell *c, const QrxAuraPodCapacity *p,
                     QrxAuraModelTier tier, int unique_provider) {
    if (unique_provider) c->provider_count++;
    c->pod_count++;
    if (p->role_mask & QRX_AURA_POD_ROLE_INFERENCE) c->inference_pods++;
    else c->utility_pods++;
    if (tier >= QRX_AURA_TIER_NANO && tier <= QRX_AURA_TIER_CLUSTER) c->tier_pods[tier]++;
    c->free_memory_bytes = sat_add64(c->free_memory_bytes, p->free_memory_bytes);
    c->model_cache_free_bytes = sat_add64(c->model_cache_free_bytes, p->model_cache_free_bytes);
    c->ai_milli_tokens_per_second = sat_add64(c->ai_milli_tokens_per_second,
                                               p->measured_milli_tokens_per_second);
    c->avg_latency_ms = sat_add32(c->avg_latency_ms, p->latency_ms);
    c->avg_utilization_bps = sat_add32(c->avg_utilization_bps, p->utilization_bps);
    c->avg_reliability_bps = sat_add32(c->avg_reliability_bps, p->reliability_bps);
    c->avg_cache_hit_bps = sat_add32(c->avg_cache_hit_bps, p->cache_hit_bps);
    c->avg_expert_locality_bps = sat_add32(c->avg_expert_locality_bps, p->expert_locality_bps);
}

static void cell_average(QrxAuraGlobeCell *c) {
    if (!c->pod_count) return;
    uint32_t n = c->pod_count > UINT32_MAX ? UINT32_MAX : (uint32_t)c->pod_count;
    c->avg_latency_ms /= n;
    c->avg_utilization_bps /= n;
    c->avg_reliability_bps /= n;
    c->avg_cache_hit_bps /= n;
    c->avg_expert_locality_bps /= n;
}

int qrx_aura_fabric_build(const QrxAuraPodCapacity *pods, size_t pod_count,
                          const QrxAuraModelCapabilityProfile *models, size_t model_count,
                          size_t privacy_min_providers, QrxAuraFabricSnapshot *out,
                          QrxAuraGlobeCell **cells_out, size_t *cell_count_out) {
    if (!out || !cells_out || !cell_count_out || (pod_count && !pods) || (model_count && !models) ||
        pod_count > QRX_AURA_FABRIC_MAX_PODS || model_count > QRX_AURA_FABRIC_MAX_MODELS) return -1;

    *cells_out = NULL;
    *cell_count_out = 0;
    memset(out, 0, sizeof(*out));
    out->version = QRX_AURA_FABRIC_VERSION;
    if (!privacy_min_providers) privacy_min_providers = QRX_AURA_FABRIC_DEFAULT_PRIVACY_MIN_PROVIDERS;

    for (size_t i = 0; i < pod_count; i++) {
        if (qrx_aura_pod_capacity_validate(&pods[i])) return -2;
        if (pod_seen_before(pods, i, i)) return -3;
    }
    for (size_t i = 0; i < model_count; i++) {
        if (qrx_aura_model_profile_validate(&models[i])) return -4;
    }

    QrxAuraGlobeCell *cells = calloc(pod_count ? pod_count : 1, sizeof(*cells));
    if (!cells) return -5;
    size_t cell_count = 0;
    uint32_t samples = 0;

    for (size_t i = 0; i < pod_count; i++) {
        if (!pods[i].available) continue;
        QrxAuraModelTier tier = qrx_aura_pod_tier_classify(&pods[i]);
        if (!provider_seen_before(pods, i, i, NULL)) out->provider_count++;
        out->total_pods++;
        if (pods[i].role_mask & QRX_AURA_POD_ROLE_INFERENCE) out->inference_pods++;
        else out->utility_pods++;
        if (tier >= QRX_AURA_TIER_NANO && tier <= QRX_AURA_TIER_CLUSTER) out->tier_pods[tier]++;
        out->free_memory_bytes = sat_add64(out->free_memory_bytes, pods[i].free_memory_bytes);
        out->model_cache_free_bytes = sat_add64(out->model_cache_free_bytes, pods[i].model_cache_free_bytes);
        out->ai_milli_tokens_per_second = sat_add64(out->ai_milli_tokens_per_second,
                                                     pods[i].measured_milli_tokens_per_second);
        out->avg_latency_ms = sat_add32(out->avg_latency_ms, pods[i].latency_ms);
        out->avg_utilization_bps = sat_add32(out->avg_utilization_bps, pods[i].utilization_bps);
        out->avg_reliability_bps = sat_add32(out->avg_reliability_bps, pods[i].reliability_bps);
        out->avg_cache_hit_bps = sat_add32(out->avg_cache_hit_bps, pods[i].cache_hit_bps);
        out->avg_expert_locality_bps = sat_add32(out->avg_expert_locality_bps,
                                                  pods[i].expert_locality_bps);
        samples++;

        size_t cell_index = 0;
        for (; cell_index < cell_count; cell_index++) {
            if (!strcmp(cells[cell_index].region, pods[i].region)) break;
        }
        if (cell_index == cell_count) {
            cells[cell_index].version = QRX_AURA_FABRIC_VERSION;
            snprintf(cells[cell_index].region, sizeof(cells[cell_index].region), "%s", pods[i].region);
            cell_count++;
        }
        cell_add(&cells[cell_index], &pods[i], tier,
                 !provider_seen_before(pods, i, i, pods[i].region));
    }

    if (samples) {
        out->avg_latency_ms /= samples;
        out->avg_utilization_bps /= samples;
        out->avg_reliability_bps /= samples;
        out->avg_cache_hit_bps /= samples;
        out->avg_expert_locality_bps /= samples;
    }

    out->k2_readiness_bps = best_tier_readiness(QRX_AURA_TIER_K2_CLASS, models, model_count,
                                                 pods, pod_count);
    out->k3_readiness_bps = best_tier_readiness(QRX_AURA_TIER_K3_CLASS, models, model_count,
                                                 pods, pod_count);
    out->max_ready_tier = QRX_AURA_TIER_NONE;
    for (size_t i = 0; i < model_count; i++) {
        uint32_t readiness = qrx_aura_model_readiness_bps(&models[i], pods, pod_count, NULL, NULL);
        if (readiness == 10000u && models[i].tier > out->max_ready_tier) out->max_ready_tier = models[i].tier;
    }

    for (size_t j = 0; j < cell_count; j++) {
        cell_average(&cells[j]);
        cells[j].publicly_visible = cells[j].provider_count >= privacy_min_providers;
        QrxAuraPodCapacity *regional = calloc(cells[j].pod_count ? cells[j].pod_count : 1,
                                              sizeof(*regional));
        if (!regional) {
            free(cells);
            return -6;
        }
        size_t regional_count = 0;
        for (size_t i = 0; i < pod_count; i++) {
            if (pods[i].available && !strcmp(pods[i].region, cells[j].region)) {
                regional[regional_count++] = pods[i];
            }
        }
        cells[j].k2_readiness_bps = best_tier_readiness(QRX_AURA_TIER_K2_CLASS, models, model_count,
                                                         regional, regional_count);
        cells[j].k3_readiness_bps = best_tier_readiness(QRX_AURA_TIER_K3_CLASS, models, model_count,
                                                         regional, regional_count);
        free(regional);
        if (cells[j].publicly_visible) out->visible_regions++;
        else out->hidden_regions++;
    }

    *cells_out = cells;
    *cell_count_out = cell_count;
    return 0;
}

void qrx_aura_globe_free(QrxAuraGlobeCell *cells) {
    free(cells);
}

static uint32_t task_quality(const QrxAuraModelCapabilityProfile *m, QrxAuraTaskClass task_class) {
    switch (task_class) {
        case QRX_AURA_TASK_CODING: return m->coding_bps;
        case QRX_AURA_TASK_REASONING: return m->reasoning_bps;
        case QRX_AURA_TASK_RAG: return m->rag_bps;
        case QRX_AURA_TASK_TOOL_USE: return m->tool_use_bps;
        default: return m->quality_bps;
    }
}

static uint32_t derived_quality(const QrxAuraRouteRequest *r) {
    if (r->min_quality_bps) return r->min_quality_bps;
    uint64_t q = 3000ULL + (uint64_t)r->complexity_bps * 55ULL / 100ULL;
    if (r->task_class == QRX_AURA_TASK_CODING) q += 500ULL;
    if (r->task_class == QRX_AURA_TASK_REASONING) q += 800ULL;
    if (q > 9500ULL) q = 9500ULL;
    return (uint32_t)q;
}

static uint32_t model_fitness(const QrxAuraModelCapabilityProfile *m,
                              const QrxAuraPodCapacity *pods, size_t pod_count,
                              uint32_t *selected_pods_out, uint64_t *selected_tps_out) {
    uint32_t selected_pods = 0;
    uint64_t selected_tps = 0;
    uint32_t readiness = qrx_aura_model_readiness_bps(m, pods, pod_count,
                                                       &selected_pods, &selected_tps);
    if (!readiness) return 0;

    uint64_t memory = 0;
    uint32_t reliability = 0;
    uint32_t latency = 0;
    uint32_t cache_hit = 0;
    uint32_t locality = 0;
    uint32_t eligible = 0;

    if (!m->is_moe && m->supports_single_device) {
        uint32_t single_readiness = 0;
        const QrxAuraPodCapacity *p = single_device_best(m, pods, pod_count, &single_readiness);
        if (p && single_readiness) {
            memory = p->free_memory_bytes;
            reliability = p->reliability_bps;
            latency = p->latency_ms;
            cache_hit = p->cache_hit_bps;
            locality = p->expert_locality_bps;
            selected_tps = p->measured_milli_tokens_per_second;
            selected_pods = 1;
            eligible = 1;
        }
    } else {
        for (size_t i = 0; i < pod_count; i++) {
            if (qrx_aura_pod_capacity_validate(&pods[i]) || !basic_pod_eligible(&pods[i], m)) continue;
            memory = sat_add64(memory, pods[i].free_memory_bytes);
            reliability = sat_add32(reliability, pods[i].reliability_bps);
            latency = sat_add32(latency, pods[i].latency_ms);
            cache_hit = sat_add32(cache_hit, pods[i].cache_hit_bps);
            locality = sat_add32(locality, pods[i].expert_locality_bps);
            eligible++;
        }
        if (eligible) {
            reliability /= eligible;
            latency /= eligible;
            cache_hit /= eligible;
            locality /= eligible;
        }
    }

    if (!eligible) return 0;
    uint32_t memory_score = ratio_bps64(memory, m->recommended_memory_bytes);
    uint64_t tps_target = m->min_aggregate_milli_tokens_per_second > UINT64_MAX / 2ULL
                        ? UINT64_MAX : m->min_aggregate_milli_tokens_per_second * 2ULL;
    uint32_t tps_score = ratio_bps64(selected_tps, tps_target);
    uint32_t latency_score = 10000u;
    if (m->max_latency_ms) {
        latency_score = latency >= m->max_latency_ms ? 0u
                      : (uint32_t)(((uint64_t)(m->max_latency_ms - latency) * 10000ULL) /
                                   m->max_latency_ms);
    }
    uint64_t score = (uint64_t)readiness * 30ULL +
                     (uint64_t)memory_score * 15ULL +
                     (uint64_t)tps_score * 20ULL +
                     (uint64_t)reliability * 12ULL +
                     (uint64_t)latency_score * 8ULL +
                     (uint64_t)cache_hit * 7ULL +
                     (uint64_t)locality * 8ULL;
    if (selected_pods_out) *selected_pods_out = selected_pods;
    if (selected_tps_out) *selected_tps_out = selected_tps;
    return (uint32_t)(score / 100ULL);
}

static const QrxAuraModelCapabilityProfile *find_model(const QrxAuraModelCapabilityProfile *models,
                                                       size_t model_count,
                                                       const char *model_id,
                                                       const char *model_version) {
    for (size_t i = 0; i < model_count; i++) {
        if (!strcmp(models[i].model_id, model_id) &&
            !strcmp(models[i].model_version, model_version)) return &models[i];
    }
    return NULL;
}

static int digest_u32(EVP_MD_CTX *ctx, uint32_t v) {
    unsigned char b[4] = {
        (unsigned char)(v >> 24), (unsigned char)(v >> 16),
        (unsigned char)(v >> 8), (unsigned char)v
    };
    return EVP_DigestUpdate(ctx, b, sizeof(b)) == 1 ? 0 : -1;
}

static int digest_u64(EVP_MD_CTX *ctx, uint64_t v) {
    unsigned char b[8];
    for (int i = 7; i >= 0; i--) {
        b[i] = (unsigned char)(v & 255u);
        v >>= 8;
    }
    return EVP_DigestUpdate(ctx, b, sizeof(b)) == 1 ? 0 : -1;
}

static int digest_field(EVP_MD_CTX *ctx, const char *s) {
    size_t n = strlen(s);
    if (digest_u64(ctx, (uint64_t)n)) return -1;
    return n && EVP_DigestUpdate(ctx, s, n) != 1 ? -1 : 0;
}

int qrx_aura_route_decision_commitment(const QrxAuraRouteDecision *d, char out[65]) {
    if (!d || !out || d->version != QRX_AURA_FABRIC_VERSION ||
        !bounded_nonempty(d->model_id, sizeof(d->model_id)) ||
        !bounded_nonempty(d->model_version, sizeof(d->model_version)) ||
        !bounded_nonempty(d->runtime_id, sizeof(d->runtime_id)) ||
        d->tier < QRX_AURA_TIER_NANO || d->tier > QRX_AURA_TIER_K3_CLASS ||
        d->mode < QRX_AURA_ROUTE_AUTO || d->mode > QRX_AURA_ROUTE_PINNED ||
        d->task_class < QRX_AURA_TASK_CHAT || d->task_class > QRX_AURA_TASK_TOOL_USE) return -1;

    static const char domain[] = "QRX/AURA/MODEL-ROUTE/V1";
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned char hash[32];
    unsigned int hash_len = 0;
    int ok = ctx != NULL;
    if (ok && EVP_DigestInit_ex(ctx, EVP_sha3_256(), NULL) != 1) ok = 0;
    if (ok && EVP_DigestUpdate(ctx, domain, sizeof(domain) - 1) != 1) ok = 0;
    if (ok && digest_field(ctx, d->model_id)) ok = 0;
    if (ok && digest_field(ctx, d->model_version)) ok = 0;
    if (ok && digest_field(ctx, d->runtime_id)) ok = 0;
    if (ok && digest_u32(ctx, d->mode)) ok = 0;
    if (ok && digest_u32(ctx, d->task_class)) ok = 0;
    if (ok && digest_u32(ctx, d->tier)) ok = 0;
    if (ok && digest_u32(ctx, d->fitness_bps)) ok = 0;
    if (ok && digest_u32(ctx, d->readiness_bps)) ok = 0;
    if (ok && digest_u32(ctx, d->task_quality_bps)) ok = 0;
    if (ok && digest_u32(ctx, d->required_quality_bps)) ok = 0;
    if (ok && digest_u32(ctx, d->selected_pods)) ok = 0;
    if (ok && digest_u64(ctx, d->aggregate_milli_tokens_per_second)) ok = 0;
    if (ok && digest_u32(ctx, d->degraded)) ok = 0;
    if (ok && EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) ok = 0;
    EVP_MD_CTX_free(ctx);
    if (!ok || hash_len != 32) return -2;

    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i * 2] = hex[hash[i] >> 4];
        out[i * 2 + 1] = hex[hash[i] & 15];
    }
    out[64] = 0;
    return 0;
}

int qrx_aura_route_model(const QrxAuraRouteRequest *r,
                         const QrxAuraModelCapabilityProfile *models, size_t model_count,
                         const QrxAuraPodCapacity *pods, size_t pod_count,
                         QrxAuraRouteDecision *out) {
    if (!r || !out || !models || !model_count || (pod_count && !pods) ||
        r->version != QRX_AURA_FABRIC_VERSION ||
        model_count > QRX_AURA_FABRIC_MAX_MODELS || pod_count > QRX_AURA_FABRIC_MAX_PODS ||
        r->mode < QRX_AURA_ROUTE_AUTO || r->mode > QRX_AURA_ROUTE_PINNED ||
        r->task_class < QRX_AURA_TASK_CHAT || r->task_class > QRX_AURA_TASK_TOOL_USE ||
        !bps_ok(r->complexity_bps) || !bps_ok(r->min_quality_bps)) return -1;

    for (size_t i = 0; i < model_count; i++) {
        if (qrx_aura_model_profile_validate(&models[i])) return -2;
    }
    for (size_t i = 0; i < pod_count; i++) {
        if (qrx_aura_pod_capacity_validate(&pods[i])) return -3;
    }

    uint32_t required_quality = derived_quality(r);
    const QrxAuraModelCapabilityProfile *best = NULL;
    uint32_t best_fitness = 0;
    uint32_t best_quality = 0;
    uint32_t best_readiness = 0;
    uint32_t best_pods = 0;
    uint64_t best_tps = 0;
    int degraded = 0;

    if (r->mode == QRX_AURA_ROUTE_PINNED) {
        if (!r->pinned_model_id[0] || !r->pinned_model_version[0]) return -4;
        best = find_model(models, model_count, r->pinned_model_id, r->pinned_model_version);
        if (!best) return -5;
        best_readiness = qrx_aura_model_readiness_bps(best, pods, pod_count, &best_pods, &best_tps);
        if (best_readiness < 10000u) return -6;
        if (r->min_context_tokens && best->max_context_tokens < r->min_context_tokens) return -7;
        best_quality = task_quality(best, r->task_class);
        best_fitness = model_fitness(best, pods, pod_count, &best_pods, &best_tps);
        degraded = best_quality < required_quality;
    } else {
        QrxAuraModelTier selected_tier = QRX_AURA_TIER_NONE;
        for (size_t i = 0; i < model_count; i++) {
            const QrxAuraModelCapabilityProfile *m = &models[i];
            if (r->min_context_tokens && m->max_context_tokens < r->min_context_tokens) continue;
            uint32_t selected_pods = 0;
            uint64_t selected_tps = 0;
            uint32_t readiness = qrx_aura_model_readiness_bps(m, pods, pod_count,
                                                               &selected_pods, &selected_tps);
            if (readiness < 10000u) continue;
            uint32_t quality = task_quality(m, r->task_class);
            if (quality < required_quality) continue;
            uint32_t fitness = model_fitness(m, pods, pod_count, &selected_pods, &selected_tps);
            if (!best || m->tier < selected_tier ||
                (m->tier == selected_tier && fitness > best_fitness)) {
                best = m;
                selected_tier = m->tier;
                best_fitness = fitness;
                best_quality = quality;
                best_readiness = readiness;
                best_pods = selected_pods;
                best_tps = selected_tps;
            }
        }

        if (!best && r->allow_degradation) {
            for (size_t i = 0; i < model_count; i++) {
                const QrxAuraModelCapabilityProfile *m = &models[i];
                if (r->min_context_tokens && m->max_context_tokens < r->min_context_tokens) continue;
                uint32_t selected_pods = 0;
                uint64_t selected_tps = 0;
                uint32_t readiness = qrx_aura_model_readiness_bps(m, pods, pod_count,
                                                                   &selected_pods, &selected_tps);
                if (readiness < 10000u) continue;
                uint32_t quality = task_quality(m, r->task_class);
                uint32_t fitness = model_fitness(m, pods, pod_count, &selected_pods, &selected_tps);
                if (!best || quality > best_quality ||
                    (quality == best_quality && fitness > best_fitness)) {
                    best = m;
                    best_fitness = fitness;
                    best_quality = quality;
                    best_readiness = readiness;
                    best_pods = selected_pods;
                    best_tps = selected_tps;
                }
            }
            degraded = best != NULL;
        }
        if (!best) return -8;
    }

    memset(out, 0, sizeof(*out));
    out->version = QRX_AURA_FABRIC_VERSION;
    out->mode = r->mode;
    out->task_class = r->task_class;
    snprintf(out->model_id, sizeof(out->model_id), "%s", best->model_id);
    snprintf(out->model_version, sizeof(out->model_version), "%s", best->model_version);
    snprintf(out->runtime_id, sizeof(out->runtime_id), "%s", best->runtime_id);
    out->tier = best->tier;
    out->fitness_bps = best_fitness;
    out->readiness_bps = best_readiness;
    out->task_quality_bps = best_quality;
    out->required_quality_bps = required_quality;
    out->selected_pods = best_pods;
    out->aggregate_milli_tokens_per_second = best_tps;
    out->degraded = degraded ? 1 : 0;
    if (qrx_aura_route_decision_commitment(out, out->decision_commitment)) return -9;
    return 0;
}

int qrx_aura_route_apply_to_job_node(const QrxAuraRouteDecision *d,
                                     const QrxAuraModelCapabilityProfile *models,
                                     size_t model_count, QrxComputeJobNode *node) {
    if (!d || !models || !model_count || !node || d->version != QRX_AURA_FABRIC_VERSION) return -1;
    const QrxAuraModelCapabilityProfile *m = find_model(models, model_count,
                                                        d->model_id, d->model_version);
    if (!m || strcmp(m->runtime_id, d->runtime_id) || m->tier != d->tier) return -2;
    if (node->job_type != QRX_JOB_AI_INFERENCE && node->job_type != QRX_JOB_CODE_GENERATION &&
        node->job_type != QRX_JOB_RESEARCH) return -3;
    snprintf(node->runtime_id, sizeof(node->runtime_id), "%s", d->runtime_id);
    snprintf(node->model_id, sizeof(node->model_id), "%s", d->model_id);
    snprintf(node->model_version, sizeof(node->model_version), "%s", d->model_version);
    if (node->min_memory_bytes < m->min_memory_bytes) node->min_memory_bytes = m->min_memory_bytes;
    node->capability_mask |= QRX_JOB_CAP_MODEL_INFERENCE;
    return 0;
}
