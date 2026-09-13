#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_runtime_plugin.h"
#include "compute/qrx_aura_provider_service.h"
#include "resource/qrx_resource_provider_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_PROVIDER_BOOTSTRAP_VERSION 1u

typedef struct {
    uint32_t version;
    uint8_t user_enabled; /* explicit host/wallet opt-in gate */
    char provider_id[QRX_RESOURCE_PROVIDER_ID_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char network[QRX_PROVIDER_NETWORK_MAX+1];
    char region[QRX_GLOBE_REGION_MAX+1];
    QrxMoeCalibrationProfile calibration;
    uint32_t compute_threads;
    uint64_t model_cache_bytes;
    uint64_t free_memory_bytes;
    uint64_t network_egress_mbps;
    uint8_t require_wallet_approval;
    const QrxResourceProviderWalletApproval *wallet_approval;

    const QrxAiModelRegistry *model_registry;
    QrxStorageFs *model_cache_fs;
    const char *runtime_plugin_path; /* optional when fallback_adapter is supplied */
    QrxMoeWorkerAdapter fallback_adapter;
    QrxAuraModelCacheFetchFn model_fetch;
    void *model_fetch_ctx;

    QrxAuraRemoteKeyLookupFn requester_key_lookup;
    void *requester_key_ctx;
    EVP_PKEY *provider_private_key;
    QrxAuraRemoteHeightFn height_fn;
    void *height_ctx;
    char listen_host[QRX_AURA_PROVIDER_HOST_MAX];
    uint16_t listen_port;
    char lease_journal_path[1024];
    uint8_t require_secure_dispatch;
    uint8_t enable_pq_hybrid_sessions;
    uint64_t pq_session_ttl_blocks;
    QrxAuraProviderAdvertiseFn advertise;
    void *advertise_ctx;
} QrxAuraProviderBootstrapConfig;

typedef struct {
    QrxResourceProviderRuntime runtime;
    QrxResourceProviderMarketRegistration registration;
    QrxAuraRuntimeBinding binding;
    QrxAuraModelCacheCatalog cache_catalog;
    QrxAuraRuntimePluginHost plugin;
    uint8_t plugin_open;
    QrxAuraPodCapacity advertised_capacity;
    QrxAuraProviderService *service;
} QrxAuraProviderBootstrap;

int qrx_aura_provider_bootstrap_start(const QrxAuraProviderBootstrapConfig *config,QrxAuraProviderBootstrap *out);
int qrx_aura_provider_bootstrap_stop(QrxAuraProviderBootstrap *bootstrap);
const char *qrx_aura_provider_bootstrap_endpoint(const QrxAuraProviderBootstrap *bootstrap);
const QrxResourceProviderRuntime *qrx_aura_provider_bootstrap_runtime(const QrxAuraProviderBootstrap *bootstrap);
const QrxAuraPodCapacity *qrx_aura_provider_bootstrap_capacity(const QrxAuraProviderBootstrap *bootstrap);

#ifdef __cplusplus
}
#endif
