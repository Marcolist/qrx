#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_RESOURCE_GLOBE_VERSION 1u
#define QRX_GLOBE_REGION_MAX 63u
#define QRX_GLOBE_PROVIDER_MAX 128u
#define QRX_GLOBE_DEFAULT_PRIVACY_MIN_PROVIDERS 3u

#define QRX_GLOBE_LAYER_STORAGE     (1u<<0)
#define QRX_GLOBE_LAYER_COMPUTE     (1u<<1)
#define QRX_GLOBE_LAYER_AI          (1u<<2)
#define QRX_GLOBE_LAYER_MODEL_CACHE (1u<<3)
#define QRX_GLOBE_LAYER_NETWORK     (1u<<4)
#define QRX_GLOBE_LAYER_OPPORTUNITY (1u<<5)
#define QRX_GLOBE_LAYER_ALL         ((1u<<6)-1u)

typedef enum {
    QRX_GLOBE_CAPACITY_NONE = 0,
    QRX_GLOBE_CAPACITY_LOW,
    QRX_GLOBE_CAPACITY_MEDIUM,
    QRX_GLOBE_CAPACITY_HIGH,
    QRX_GLOBE_CAPACITY_EXTREME
} QrxGlobeCapacityClass;

typedef enum {
    QRX_GLOBE_LATENCY_UNKNOWN = 0,
    QRX_GLOBE_LATENCY_LOCAL,
    QRX_GLOBE_LATENCY_LOW,
    QRX_GLOBE_LATENCY_MEDIUM,
    QRX_GLOBE_LATENCY_HIGH
} QrxGlobeLatencyClass;

typedef enum {
    QRX_GLOBE_DEMAND_UNKNOWN = 0,
    QRX_GLOBE_DEMAND_LOW,
    QRX_GLOBE_DEMAND_BALANCED,
    QRX_GLOBE_DEMAND_HIGH,
    QRX_GLOBE_DEMAND_CRITICAL
} QrxGlobeDemandClass;

typedef enum {
    QRX_GLOBE_OPPORTUNITY_NONE = 0,
    QRX_GLOBE_OPPORTUNITY_LOW,
    QRX_GLOBE_OPPORTUNITY_MEDIUM,
    QRX_GLOBE_OPPORTUNITY_HIGH,
    QRX_GLOBE_OPPORTUNITY_EXTREME
} QrxGlobeOpportunityClass;

typedef struct {
    uint32_t version;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1]; /* internal aggregation only; never emitted by public cell */
    char region[QRX_GLOBE_REGION_MAX+1];       /* coarse region/cell; no coordinates */
    uint32_t layer_mask;
    uint64_t storage_free_bytes;
    uint64_t compute_ncu_milli;
    uint64_t ai_milli_tokens_per_second;
    uint64_t model_cache_free_bytes;
    uint64_t network_egress_mbps;
    uint32_t latency_ms;
    uint32_t utilization_bps;
    uint32_t reliability_bps;
    uint32_t demand_bps;
} QrxResourceGlobeObservation;

typedef struct {
    uint32_t version;
    char region[QRX_GLOBE_REGION_MAX+1];
    uint32_t layer_mask;
    uint64_t provider_count;
    uint64_t storage_free_bytes;
    uint64_t compute_ncu_milli;
    uint64_t ai_milli_tokens_per_second;
    uint64_t model_cache_free_bytes;
    uint64_t network_egress_mbps;
    uint32_t avg_latency_ms;
    uint32_t avg_utilization_bps;
    uint32_t avg_reliability_bps;
    uint32_t avg_demand_bps;
    uint32_t storage_opportunity_score;
    uint32_t compute_opportunity_score;
    uint32_t ai_opportunity_score;
    uint32_t model_cache_opportunity_score;
    uint32_t network_opportunity_score;
    uint32_t opportunity_score;
    QrxGlobeCapacityClass capacity_class;
    QrxGlobeLatencyClass latency_class;
    QrxGlobeDemandClass demand_class;
    QrxGlobeOpportunityClass opportunity_class;
    int publicly_visible;
} QrxResourceGlobeCell;

int qrx_resource_globe_observation_validate(const QrxResourceGlobeObservation *o);
int qrx_resource_globe_build(const QrxResourceGlobeObservation *obs, size_t count,
                             size_t privacy_min_providers,
                             QrxResourceGlobeCell **cells_out, size_t *cell_count_out);
void qrx_resource_globe_free(QrxResourceGlobeCell *cells);
QrxGlobeCapacityClass qrx_resource_globe_capacity_class(const QrxResourceGlobeCell *c);
QrxGlobeLatencyClass qrx_resource_globe_latency_class(uint32_t avg_latency_ms);
QrxGlobeDemandClass qrx_resource_globe_demand_class(uint32_t demand_bps);
QrxGlobeOpportunityClass qrx_resource_globe_opportunity_class(uint32_t score);
int qrx_resource_globe_cell_commitment(const QrxResourceGlobeCell *cell, char out_hex[65]);

#ifdef __cplusplus
}
#endif
