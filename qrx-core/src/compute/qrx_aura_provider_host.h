#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_runtime_adapters.h"
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_AURA_PROVIDER_HOST_CONFIG_VERSION 1u
#define QRX_AURA_PROVIDER_HOST_FORMAT "QRXAURA41"
#define QRX_AURA_PROVIDER_RELAY_MAX 4u

typedef struct {
    uint32_t version;
    uint8_t enabled;
    char provider_id[129];
    char pod_id[129];
    char network[32];
    char region[32];
    uint32_t compute_threads;
    uint64_t model_cache_bytes;
    uint64_t free_memory_bytes;
    uint64_t network_egress_mbps;
    uint8_t require_wallet_approval;
    uint8_t require_secure_dispatch;
    uint8_t enable_pq_hybrid_sessions;
    char listen_host[128];
    uint16_t listen_port;
    char lease_journal_path[1024];
    char job_journal_path[1024];
    QrxAuraRuntimeAdapterConfig runtime_adapter;
    char relay_endpoints[QRX_AURA_PROVIDER_RELAY_MAX][256];
    uint32_t relay_count;
    uint8_t relay_required;
} QrxAuraProviderHostConfig;

void qrx_aura_provider_host_config_defaults(QrxAuraProviderHostConfig *cfg);
int qrx_aura_provider_host_config_validate(const QrxAuraProviderHostConfig *cfg);
int qrx_aura_provider_host_config_save(const char *path,const QrxAuraProviderHostConfig *cfg);
int qrx_aura_provider_host_config_load(const char *path,QrxAuraProviderHostConfig *cfg);
#ifdef __cplusplus
}
#endif
