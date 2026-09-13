#pragma once
#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "compute/qrx_aura_runtime_packages.h"
#include "compute/qrx_aura_remote_dispatch.h"
#include "storage/qrx_storage_fs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_RUNTIME_DELIVERY_VERSION 1u

typedef struct {
    uint32_t version;
    /* Optional QRX Drive context. If present, a runtime object is requested by
     * its signed SHA3 content root before any external origin is contacted. */
    QrxAuraDriveModelFetchContext *drive;
    /* Optional local content-addressed cache for runtime packages. */
    QrxStorageFs *local_cache;
    char work_dir[1024];
    uint32_t connect_timeout_seconds;
    uint32_t transfer_timeout_seconds;
    uint8_t allow_https_origin;
    uint8_t allow_file_origin; /* developer/test only */
} QrxAuraRuntimeDeliveryContext;

typedef struct {
    const char *publisher_id;
    EVP_PKEY *public_key; /* borrowed */
} QrxAuraRuntimeSinglePublisherKey;

typedef struct {
    QrxStorageFs *model_cache; /* borrowed; used to probe the installed plugin */
    QrxAuraRuntimePluginHost plugin;
    uint8_t plugin_open;
} QrxAuraRuntimeProbeActivation;

int qrx_aura_runtime_delivery_defaults(QrxAuraRuntimeDeliveryContext *out,const char *work_dir);
int qrx_aura_runtime_fetch_drive_then_origin(void *ctx,
                                             const QrxAuraRuntimePackageAnnouncement *package,
                                             char out_downloaded_path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]);
int qrx_aura_runtime_single_publisher_lookup(void *ctx,const char *publisher_id,EVP_PKEY **out_public_key);
int qrx_aura_runtime_probe_activate(void *ctx,const QrxAuraProviderHostConfig *host,
                                    const QrxMoeRuntimeDevice *device,
                                    const QrxAuraRuntimePackageAnnouncement *package,
                                    const char *installed_plugin_path);
void qrx_aura_runtime_probe_activation_close(QrxAuraRuntimeProbeActivation *ctx);

/* Zero-touch host bootstrap. The host config remains authoritative: this only
 * installs a runtime when AURA is enabled and runtime_adapter=AUTO. */
int qrx_aura_runtime_auto_bootstrap(const char *host_config_path,
                                    const char *signed_catalog_path,
                                    uint64_t height,QrxAuraPowerProfile power,
                                    const char *install_dir,
                                    QrxAuraRuntimePackageKeyLookupFn key_lookup,void *key_ctx,
                                    QrxAuraRuntimeDeliveryContext *delivery,
                                    QrxAuraRuntimeProbeActivation *probe,
                                    QrxAuraRuntimeOneClickResult *result_out);

#ifdef __cplusplus
}
#endif
