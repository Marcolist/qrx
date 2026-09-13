#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_model_fabric.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_PRODUCTION_ROUTING_VERSION 1u
#define QRX_AURA_PRODUCTION_MAX_OFFERS 4096u
#define QRX_AURA_PRODUCTION_MAX_CAMPAIGN_TASKS 256u

typedef enum {
    QRX_AURA_OPT_ECO=1,
    QRX_AURA_OPT_BALANCED=2,
    QRX_AURA_OPT_PERFORMANCE=3
} QrxAuraProductionOptimization;

typedef struct {
    uint32_t version;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char region[QRX_GLOBE_REGION_MAX+1];
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    QrxMoeRuntimeBackend backend;
    uint64_t milli_tokens_per_second;
    uint32_t latency_ms;
    uint32_t reliability_bps;
    uint32_t observed_quality_bps;
    uint64_t price_microqub_per_million_tokens;
    uint64_t energy_mwh_per_1000_tokens;
    uint32_t wan_mbps;
    uint8_t available;
} QrxAuraProductionOffer;

typedef struct {
    uint32_t version;
    QrxAuraTaskClass task_class;
    QrxAuraProductionOptimization optimization;
    uint32_t min_quality_bps;
    uint32_t max_latency_ms;
    uint64_t max_price_microqub_per_million_tokens;
    uint64_t max_energy_mwh_per_1000_tokens;
    uint32_t min_reliability_bps;
    uint32_t min_context_tokens;
    char preferred_region[QRX_GLOBE_REGION_MAX+1];
    uint8_t allow_cross_region;
} QrxAuraProductionRouteRequest;

typedef struct {
    uint32_t version;
    uint32_t offer_index;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char region[QRX_GLOBE_REGION_MAX+1];
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    uint32_t total_score;
    uint32_t task_quality_bps;
    uint32_t observed_quality_bps;
    uint32_t latency_score_bps;
    uint32_t cost_score_bps;
    uint32_t energy_score_bps;
    uint32_t reliability_score_bps;
    uint32_t locality_score_bps;
    uint64_t price_microqub_per_million_tokens;
    uint64_t energy_mwh_per_1000_tokens;
    uint64_t milli_tokens_per_second;
    char decision_commitment[65];
} QrxAuraProductionRouteDecision;

typedef struct {
    uint32_t version;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    uint64_t requests;
    uint64_t tokens;
    uint64_t latency_ms_total;
    uint64_t quality_bps_token_sum;
    uint64_t cost_microqub_total;
    uint64_t energy_mwh_total;
    uint64_t wan_bytes;
    uint64_t failed;
} QrxAuraProductionTelemetry;

typedef struct {
    uint32_t version;
    uint64_t tasks_total;
    uint64_t tasks_routed;
    uint64_t tasks_failed;
    uint64_t cross_region_routes;
    uint64_t aggregate_score;
    uint64_t aggregate_price_microqub_per_million_tokens;
    uint64_t aggregate_energy_mwh_per_1000_tokens;
    uint64_t aggregate_latency_ms;
    char campaign_commitment[65];
} QrxAuraProductionCampaignResult;

int qrx_aura_production_offer_validate(const QrxAuraProductionOffer *offer);
int qrx_aura_production_route(const QrxAuraProductionRouteRequest *request,
                              const QrxAuraModelCapabilityProfile *models,size_t model_count,
                              const QrxAuraProductionOffer *offers,size_t offer_count,
                              QrxAuraProductionRouteDecision *out);
int qrx_aura_production_route_commitment(const QrxAuraProductionRouteDecision *decision,char out_hex[65]);
void qrx_aura_production_telemetry_init(QrxAuraProductionTelemetry *telemetry,const char *provider_id,const char *model_id,const char *model_version);
int qrx_aura_production_telemetry_record(QrxAuraProductionTelemetry *telemetry,uint64_t tokens,uint32_t latency_ms,uint32_t quality_bps,
                                         uint64_t cost_microqub,uint64_t energy_mwh,uint64_t wan_bytes,uint8_t failed);
uint32_t qrx_aura_production_telemetry_failure_bps(const QrxAuraProductionTelemetry *telemetry);
uint32_t qrx_aura_production_telemetry_avg_quality_bps(const QrxAuraProductionTelemetry *telemetry);
uint32_t qrx_aura_production_telemetry_avg_latency_ms(const QrxAuraProductionTelemetry *telemetry);
int qrx_aura_production_telemetry_commitment(const QrxAuraProductionTelemetry *telemetry,char out_hex[65]);
int qrx_aura_production_campaign_run(const QrxAuraProductionRouteRequest *tasks,size_t task_count,
                                     const QrxAuraModelCapabilityProfile *models,size_t model_count,
                                     const QrxAuraProductionOffer *offers,size_t offer_count,
                                     QrxAuraProductionCampaignResult *out);
#ifdef __cplusplus
}
#endif
