#pragma once
#include <stdint.h>
#include "resource/qrx_resource_provider.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_PROVIDER_RUNTIME_VERSION 1u
#define QRX_PROVIDER_MARKET_REG_VERSION 1u
#define QRX_PROVIDER_APPROVAL_VERSION 1u
#define QRX_PROVIDER_NETWORK_MAX 32u
#define QRX_PROVIDER_APPROVAL_ID_MAX 96u

typedef enum {
    QRX_PROVIDER_RUNTIME_CONFIGURED = 1,
    QRX_PROVIDER_RUNTIME_READY = 2,
    QRX_PROVIDER_RUNTIME_REGISTERED = 3,
    QRX_PROVIDER_RUNTIME_SERVING = 4,
    QRX_PROVIDER_RUNTIME_DRAINING = 5,
    QRX_PROVIDER_RUNTIME_OFFLINE = 6
} QrxResourceProviderRuntimeStatus;

typedef struct {
    uint32_t version;
    char approval_id[QRX_PROVIDER_APPROVAL_ID_MAX+1];
    char provider_id[QRX_RESOURCE_PROVIDER_ID_MAX+1];
    char network[QRX_PROVIDER_NETWORK_MAX+1];
    char plan_commitment[65];
    uint64_t approved_at_height;
    uint64_t expires_at_height;
    uint8_t approved;
} QrxResourceProviderWalletApproval;

typedef struct {
    uint32_t version;
    char provider_id[QRX_RESOURCE_PROVIDER_ID_MAX+1];
    char network[QRX_PROVIDER_NETWORK_MAX+1];
    char plan_commitment[65];
    uint64_t registration_revision;
    uint32_t enabled_mask;
    uint64_t storage_bytes;
    uint64_t model_cache_bytes;
    uint32_t compute_threads;
    uint64_t network_egress_mbps;
    uint8_t accelerator_enabled;
    uint8_t cuda_enabled;
    uint8_t metal_mlx_enabled;
    uint8_t wallet_approved;
} QrxResourceProviderMarketRegistration;

typedef struct {
    uint32_t version;
    QrxResourceProviderRuntimeStatus status;
    char provider_id[QRX_RESOURCE_PROVIDER_ID_MAX+1];
    char network[QRX_PROVIDER_NETWORK_MAX+1];
    QrxResourceProviderPlan active_plan;
    char active_plan_commitment[65];
    uint64_t market_revision;
    uint64_t active_jobs;
    uint64_t reserved_storage_bytes;
    uint64_t reserved_model_cache_bytes;
    uint32_t reserved_compute_threads;
    uint64_t reserved_network_egress_mbps;
    uint8_t has_pending_plan;
    QrxResourceProviderPlan pending_plan;
    char pending_plan_commitment[65];
} QrxResourceProviderRuntime;

int qrx_resource_provider_wallet_approval_validate(const QrxResourceProviderWalletApproval *approval,
                                                    const char *provider_id,
                                                    const char *network,
                                                    const char plan_commitment[65],
                                                    uint64_t current_height);
int qrx_resource_provider_runtime_init(QrxResourceProviderRuntime *runtime,
                                       const char *provider_id,
                                       const char *network,
                                       const QrxResourceProviderPlan *plan);
int qrx_resource_provider_runtime_mark_ready(QrxResourceProviderRuntime *runtime);
int qrx_resource_provider_market_register(QrxResourceProviderRuntime *runtime,
                                          const QrxResourceProviderWalletApproval *approval,
                                          uint64_t current_height,
                                          QrxResourceProviderMarketRegistration *registration);
int qrx_resource_provider_runtime_start_serving(QrxResourceProviderRuntime *runtime);
int qrx_resource_provider_runtime_reserve(QrxResourceProviderRuntime *runtime,
                                          uint64_t storage_bytes,
                                          uint64_t model_cache_bytes,
                                          uint32_t compute_threads,
                                          uint64_t network_egress_mbps);
int qrx_resource_provider_runtime_release(QrxResourceProviderRuntime *runtime,
                                          uint64_t storage_bytes,
                                          uint64_t model_cache_bytes,
                                          uint32_t compute_threads,
                                          uint64_t network_egress_mbps);
int qrx_resource_provider_runtime_request_plan(QrxResourceProviderRuntime *runtime,
                                               const QrxResourceProviderPlan *plan);
int qrx_resource_provider_runtime_try_apply_pending(QrxResourceProviderRuntime *runtime);
int qrx_resource_provider_runtime_begin_drain(QrxResourceProviderRuntime *runtime);
int qrx_resource_provider_runtime_shutdown(QrxResourceProviderRuntime *runtime);
int qrx_resource_provider_market_registration_commitment(const QrxResourceProviderMarketRegistration *registration,
                                                         char out_hex[65]);

#ifdef __cplusplus
}
#endif
